/* Wormhole OBS output: sender with v2 negotiation and multi-audio. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/transport.h"
#include "wormhole/media.h"
#include "wormhole/protocol.h"
#include "wormhole/protocol_v2.h"
#include "wormhole/queue.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <turbojpeg.h>

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WH_MAX_QUEUE 120
#define WH_MAX_AUDIO_QUEUE 240
#define WH_MAX_AUDIO_SOURCES 16

/* One OBS source captured pre-mix, mapped to one Wormhole audio stream. */
typedef struct wh_source_binding {
    obs_source_t *source; /* strong ref, released on stop */
    uint32_t track_id;
    char name[128];
    struct wh_output *ctx;
    int logged;
} wh_source_binding_t;

typedef struct wh_video_item {
    struct wh_video_item *next;
    uint8_t *yuv;
    size_t yuv_len;
    int width;
    int height;
    int subsampling;
    uint64_t timestamp;
    struct wh_video_item *pool_next;
} wh_video_item_t;

typedef struct wh_audio_item {
    struct wh_audio_item *next;
    uint8_t *pcm;
    size_t pcm_len;
    uint32_t track_id;
    uint32_t frames;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits;
    uint64_t timestamp;
    int need_desc;
    char name[128];
} wh_audio_item_t;

typedef struct wh_output {
    obs_output_t *output;
    wh_media_t media;
    wh_socket_t listener;
    wh_socket_t client;
    volatile int running;
    volatile int thread_valid;
    wh_thread_t thread_handle;
    wh_mutex_t video_mutex;
    wh_video_item_t *video_head;
    wh_video_item_t *video_tail;
    int video_count;
    wh_video_item_t *video_pool;
    wh_mutex_t audio_mutex;
    wh_audio_item_t *audio_head;
    wh_audio_item_t *audio_tail;
    int audio_count;
    int quality;
    char name[128];
    int audio_tracks;
    char track_names[6][128];
    int track_count;
    int desc_sent[16];
    char last_manifest[16 * 1024];
    wh_source_binding_t bindings[WH_MAX_AUDIO_SOURCES];
    int binding_count;
    uint32_t sequence;
    uint64_t dropped;
} wh_output_t;

static void wh_output_release_bindings(struct wh_output *ctx);
static void wh_send_manifest(struct wh_output *ctx);
#define WH_MANIFEST_CAP (16u * 1024u)

typedef struct {
    char *buf;
    char audio_names[8][128];
    int audio_count;
    int collect;
    int track_count;
} wh_manifest_ctx_t;

static void wh_build_manifest_task(void *param);

static const char *wh_output_name(void *type_data)
{
    (void)type_data;
    return "Wormhole";
}

static void wh_output_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "port", 18090);
    obs_data_set_default_int(settings, "quality", 90);
}

static obs_properties_t *wh_output_properties(void *data)
{
    (void)data;
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_int(props, "port", "TCP Port", 1024, 65535, 1);
    obs_properties_add_int_slider(props, "quality", "JPEG Quality", 1, 100, 1);
    return props;
}

/* Pre-mix, per-source audio callback. OBS hands us float-planar samples
 * before the mixer sums sources together, so each source keeps its own
 * identity all the way to the receiver. */
static void wh_source_audio_capture(void *param, obs_source_t *source,
                                    const struct audio_data *audio_data,
                                    bool muted)
{
    wh_source_binding_t *binding = param;
    wh_output_t *ctx = binding->ctx;
    wh_audio_item_t *item;
    size_t frames = audio_data->frames;
    size_t bytes;
    size_t i;
    const float *left;
    const float *right;
    enum speaker_layout layout;
    int16_t *pcm;

    (void)source;
    if (!binding->logged) {
        binding->logged = 1;
        blog(LOG_INFO, "[wormhole] audio cb '%s': frames=%u muted=%d layout=%d planes=%p/%p",
             binding->name, (unsigned)frames, (int)muted,
             (int)obs_source_get_speaker_layout(source),
             (void *)audio_data->data[0], (void *)audio_data->data[1]);
    }
    if (!ctx->running || muted || !frames || frames > 8192)
        return;
    if (ctx->audio_count >= WH_MAX_AUDIO_QUEUE)
        return;

    layout = obs_source_get_speaker_layout(source);
    left = (const float *)audio_data->data[0];
    right = (layout == SPEAKERS_MONO || !audio_data->data[1])
              ? left : (const float *)audio_data->data[1];
    bytes = frames * 2u * sizeof(int16_t);

    item = bzalloc(sizeof(*item));
    if (!item)
        return;
    item->pcm = malloc(bytes);
    if (!item->pcm) {
        bfree(item);
        return;
    }
    pcm = (int16_t *)item->pcm;
    for (i = 0; i < frames; ++i) {
        float l = left ? left[i] : 0.0f;
        float r = right ? right[i] : l;
        int li = (int)(l * 32767.0f);
        int ri = (int)(r * 32767.0f);
        if (li > 32767) li = 32767; else if (li < -32768) li = -32768;
        if (ri > 32767) ri = 32767; else if (ri < -32768) ri = -32768;
        pcm[i * 2u] = (int16_t)li;
        pcm[i * 2u + 1u] = (int16_t)ri;
    }
    item->pcm_len = bytes;
    item->track_id = binding->track_id;
    item->frames = (uint32_t)frames;
    item->sample_rate = 48000;
    item->channels = 2;
    item->bits = 16;
    item->timestamp = audio_data->timestamp;
    if (binding->track_id && binding->track_id <= 16 &&
        !ctx->desc_sent[binding->track_id - 1]) {
        item->need_desc = 1;
        ctx->desc_sent[binding->track_id - 1] = 1;
    }
    snprintf(item->name, sizeof(item->name), "%s", binding->name);

    wh_mutex_lock(&ctx->audio_mutex);
    item->next = NULL;
    if (ctx->audio_tail) ctx->audio_tail->next = item;
    else ctx->audio_head = item;
    ctx->audio_tail = item;
    ++ctx->audio_count;
    wh_mutex_unlock(&ctx->audio_mutex);
}

static WH_THREAD_API wh_output_thread(WH_THREAD_ARG arg)
{
    wh_output_t *ctx = arg;
    uint8_t *frame = (uint8_t *)malloc(4u * 1024u * 1024u);
    wh_socket_t client = WH_INVALID_SOCKET;
    size_t frame_len = 0;
    wh2_hello_t hello;
    wh2_hello_ack_t ack;
    int negotiated = 0;
    time_t last_sync = 0;
    memset(&hello, 0, sizeof(hello));
    memset(&ack, 0, sizeof(ack));

    if (!wh_net_accept(ctx->listener, &client)) {
        blog(LOG_INFO, "[wormhole] accept ended");
        free(frame);
        return WH_THREAD_RET;
    }
    if (!ctx->running) {
        wh_net_close(client);
        free(frame);
        return WH_THREAD_RET;
    }
    ctx->client = client; /* stop() owns closing this socket */
    blog(LOG_INFO, "[wormhole] receiver connected");

    /* v2 negotiation: the receiver speaks first. */
    if (!wh_net_recv_frame(client, frame, 4u * 1024u * 1024u, &frame_len)) {
        blog(LOG_WARNING, "[wormhole] failed to read HELLO");
        free(frame);
        return WH_THREAD_RET;
    }
    blog(LOG_INFO, "[wormhole] first frame %zu bytes, magic=%.4s", frame_len, (const char *)frame);
    {
        wh2_parse_status_t status = wh2_parse_hello(frame, frame_len, &hello);
        blog(LOG_INFO, "[wormhole] HELLO parse status %d", (int)status);
        if (status != WH2_PARSE_OK) {
            free(frame);
            return WH_THREAD_RET;
        }
    }
    {
        size_t n = wh2_build_hello_ack(frame, 4u * 1024u * 1024u, ++ctx->sequence,
                                       0, WH2_CAP_MULTI_AUDIO, ctx->name);
        int sent = n && wh_net_send(client, frame, n);
        blog(LOG_INFO, "[wormhole] ACK built=%zu sent=%d", n, sent);
        if (sent)
            negotiated = 1;
    }
    blog(LOG_INFO, "[wormhole] negotiation=%d, entering send loop", negotiated);
    if (negotiated)
        wh_send_manifest(ctx);
    /* Data capture is started in wh_output_start() on OBS's own thread;
     * calling OBS APIs from this worker thread races with output shutdown. */
    ctx->track_count = negotiated ? (int)hello.track_count : 1;
    /* Stream descriptors are emitted when OBS actually delivers a track. */
    (void)hello;

    while (ctx->running) {
        wh_video_item_t *video = NULL;
        wh_audio_item_t *audio = NULL;

        wh_mutex_lock(&ctx->video_mutex);
        video = ctx->video_head;
        if (video) {
            ctx->video_head = video->next;
            if (!ctx->video_head) ctx->video_tail = NULL;
            --ctx->video_count;
        }
        wh_mutex_unlock(&ctx->video_mutex);

        if (video) {
            uint8_t *jpeg = NULL;
            size_t jpeg_len = 0;
            size_t n = 0;
            if (wh_media_encode(&ctx->media, video->yuv, video->width, video->height,
                                video->subsampling, ctx->quality, &jpeg, &jpeg_len)) {
                if (negotiated) {
                    n = wh2_build_video(frame, 4u * 1024u * 1024u, ++ctx->sequence,
                                        video->timestamp, video->timestamp,
                                        video->width, video->height, jpeg, jpeg_len);
                } else {
                    wh_header_t header;
                    wh_image_header_t image;
                    memset(&header, 0, sizeof(header));
                    memset(&image, 0, sizeof(image));
                    wh_kind_to_bytes(WH_KIND_JPEG, header.type);
                    header.kind = WH_KIND_JPEG;
                    header.timestamp = (int64_t)video->timestamp;
                    header.size = (int32_t)jpeg_len;
                    image.color_range_min[0] = 0; image.color_range_min[1] = 0; image.color_range_min[2] = 0;
                    image.color_range_max[0] = 1; image.color_range_max[1] = 1; image.color_range_max[2] = 1;
                    wh_header_encode(frame, 4u * 1024u * 1024u, &header);
                    wh_image_header_encode(frame + WH_HEADER_LEN, 4u * 1024u * 1024u - WH_HEADER_LEN, &image);
                    memcpy(frame + WH_HEADER_LEN + WH_IMAGE_HEADER_LEN, jpeg, jpeg_len);
                    n = WH_HEADER_LEN + WH_IMAGE_HEADER_LEN + jpeg_len;
                }
                if (n && !wh_net_send(client, frame, n))
                    ctx->running = 0;
                wh_media_free_jpeg(jpeg);
            }
            wh_mutex_lock(&ctx->video_mutex);
            video->pool_next = ctx->video_pool;
            ctx->video_pool = video;
            wh_mutex_unlock(&ctx->video_mutex);
        }

        wh_mutex_lock(&ctx->audio_mutex);
        audio = ctx->audio_head;
        if (audio) {
            ctx->audio_head = audio->next;
            if (!ctx->audio_head) ctx->audio_tail = NULL;
            --ctx->audio_count;
        }
        wh_mutex_unlock(&ctx->audio_mutex);

        if (audio) {
            size_t n = 0;
            if (negotiated) {
                if (audio->need_desc) {
                    n = wh2_build_stream_desc(frame, 4u * 1024u * 1024u, ++ctx->sequence,
                                              audio->timestamp, audio->track_id, audio->name,
                                              audio->sample_rate, audio->channels,
                                              audio->bits, 1, 0);
                    blog(LOG_INFO, "[wormhole] audio stream %u: '%s'",
                         (unsigned)audio->track_id, audio->name);
                    if (!n || !wh_net_send(client, frame, n))
                        ctx->running = 0;
                    audio->need_desc = 0;
                }
                n = wh2_build_audio(frame, 4u * 1024u * 1024u, ++ctx->sequence,
                                    audio->timestamp, audio->track_id,
                                    audio->frames, audio->sample_rate,
                                    audio->channels, audio->bits,
                                    audio->pcm, audio->pcm_len);
            } else {
                /* Teleport compatibility: first track only, packed WAVE. */
                wh_header_t header;
                wh_wave_header_t wave;
                memset(&header, 0, sizeof(header));
                memset(&wave, 0, sizeof(wave));
                wh_kind_to_bytes(WH_KIND_WAVE, header.type);
                header.kind = WH_KIND_WAVE;
                header.timestamp = (int64_t)audio->timestamp;
                header.size = (int32_t)audio->pcm_len;
                wave.format = 4;
                wave.sample_rate = (int32_t)audio->sample_rate;
                wave.speakers = (int32_t)audio->channels;
                wave.frames = (int32_t)audio->frames;
                wh_header_encode(frame, 4u * 1024u * 1024u, &header);
                wh_wave_header_encode(frame + WH_HEADER_LEN, 4u * 1024u * 1024u - WH_HEADER_LEN, &wave);
                memcpy(frame + WH_HEADER_LEN + WH_WAVE_HEADER_LEN, audio->pcm, audio->pcm_len);
                n = WH_HEADER_LEN + WH_WAVE_HEADER_LEN + audio->pcm_len;
            }
            if (n && !wh_net_send(client, frame, n))
                ctx->running = 0;
            free(audio->pcm);
            bfree(audio);
        }

        if (!video && !audio) {
            wh_sleep_ms(1);
        }
        if (negotiated) {
            time_t now = time(NULL);
            if (now != last_sync) {
                last_sync = now;
                /* Full-state diff: scene switch, mute and visibility all
                 * arrive as a manifest change, no incremental events needed. */
                wh_manifest_ctx_t m;
                memset(&m, 0, sizeof(m));
                m.track_count = ctx->audio_tracks;
                m.buf = bzalloc(WH_MANIFEST_CAP);
                if (m.buf) {
                    obs_queue_task(OBS_TASK_UI, wh_build_manifest_task, &m, true);
                    if (strcmp(m.buf, ctx->last_manifest) != 0) {
                        size_t n = wh2_build_text(frame, 4u * 1024u * 1024u,
                                                  WH2_MSG_SCENE_MANIFEST,
                                                  ++ctx->sequence, 0, m.buf);
                        if (n && wh_net_send(client, frame, n)) {
                            snprintf(ctx->last_manifest, sizeof(ctx->last_manifest),
                                     "%s", m.buf);
                            blog(LOG_INFO, "[wormhole] scene state updated (%zu bytes)",
                                 strlen(m.buf));
                        }
                    }
                    bfree(m.buf);
                }
            }
        }
    }

    /* The client socket is closed by wh_output_stop(), which owns the
     * join; closing it here as well caused a double-close race. */
    free(frame);
    (void)ack;
    return WH_THREAD_RET;
}

static void *wh_output_create(obs_data_t *settings, obs_output_t *output)
{
    wh_output_t *ctx = bzalloc(sizeof(*ctx));
    ctx->output = output;
    ctx->quality = (int)obs_data_get_int(settings, "quality");
    {
        const char *name = obs_data_get_string(settings, "name");
        snprintf(ctx->name, sizeof(ctx->name), "%s", name ? name : "Wormhole Sender");
    }
    ctx->track_count = 0; /* tracks are discovered from OBS callbacks */
    ctx->listener = WH_INVALID_SOCKET;
    ctx->client = WH_INVALID_SOCKET;
    wh_mutex_init(&ctx->video_mutex);
    wh_mutex_init(&ctx->audio_mutex);
    wh_media_init(&ctx->media);
    return ctx;
}

static void wh_output_destroy(void *data)
{
    wh_output_t *ctx = data;
    if (!ctx) return;
    wh_output_release_bindings(ctx);
    wh_media_free(&ctx->media);
    wh_mutex_free(&ctx->video_mutex);
    wh_mutex_free(&ctx->audio_mutex);
    bfree(ctx);
}

/* Return queued items to their pools / free them. Called only after the
 * worker thread has been joined, so no producer can touch the queues. */
static void wh_output_drain(wh_output_t *ctx)
{
    wh_mutex_lock(&ctx->video_mutex);
    while (ctx->video_head) {
        wh_video_item_t *v = ctx->video_head;
        ctx->video_head = v->next;
        v->pool_next = ctx->video_pool;
        ctx->video_pool = v;
    }
    ctx->video_tail = NULL;
    ctx->video_count = 0;
    wh_mutex_unlock(&ctx->video_mutex);

    wh_mutex_lock(&ctx->audio_mutex);
    while (ctx->audio_head) {
        wh_audio_item_t *a = ctx->audio_head;
        ctx->audio_head = a->next;
        free(a->pcm);
        bfree(a);
    }
    ctx->audio_tail = NULL;
    ctx->audio_count = 0;
    wh_mutex_unlock(&ctx->audio_mutex);
}

/* Called on OBS's UI thread from wh_output_start(). Bind every audio source
 * so its frames are captured individually, before the mixer sums them. */
/* Track names: prefer OBS's own AdvOut Track<N>Name values; fall back to
 * the sender's audio source names, then to "Track N". Called on UI thread. */
static bool wh_collect_audio_name(void *param, obs_source_t *source)
{
    char (*names)[128] = param;
    int i;
    if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
        return true;
    for (i = 0; i < 6; ++i) {
        if (!names[i][0]) {
            snprintf(names[i], 128, "%s", obs_source_get_name(source));
            break;
        }
    }
    return true;
}

static void wh_init_track_names(wh_output_t *ctx)
{
    config_t *cfg = obs_frontend_get_profile_config();
    char fallback[6][128];
    int t;

    memset(fallback, 0, sizeof(fallback));
    obs_enum_sources(wh_collect_audio_name, fallback);
    for (t = 0; t < 6; ++t) {
        char key[32];
        const char *val = NULL;
        snprintf(key, sizeof(key), "Track%dName", t + 1);
        if (cfg)
            val = config_get_string(cfg, "AdvOut", key);
        if (val && *val)
            snprintf(ctx->track_names[t], 128, "%s", val);
        else if (fallback[t][0])
            snprintf(ctx->track_names[t], 128, "%s", fallback[t]);
        else
            snprintf(ctx->track_names[t], 128, "Track %d", t + 1);
    }
}

static bool wh_enum_audio_source(void *param, obs_source_t *source)
{
    wh_output_t *ctx = param;
    uint32_t flags = obs_source_get_output_flags(source);
    const char *id;
    wh_source_binding_t *b;

    if (!(flags & OBS_SOURCE_AUDIO))
        return true;
    if (ctx->binding_count >= WH_MAX_AUDIO_SOURCES)
        return true;
    id = obs_source_get_id(source);
    if (id && strncmp(id, "wormhole", 8) == 0)
        return true; /* never capture our own receiver clones */

    b = &ctx->bindings[ctx->binding_count];
    b->source = obs_source_get_ref(source);
    if (!b->source)
        return true;
    b->ctx = ctx;
    b->track_id = (uint32_t)ctx->binding_count + 1u;
    snprintf(b->name, sizeof(b->name), "%s",
             obs_source_get_name(source) ? obs_source_get_name(source) : "Audio");
    obs_source_add_audio_capture_callback(b->source, wh_source_audio_capture, b);
    ++ctx->binding_count;
    blog(LOG_INFO, "[wormhole] bound audio source %u: '%s'", b->track_id, b->name);
    return true;
}

static void wh_output_release_bindings(wh_output_t *ctx)
{
    int i;
    for (i = 0; i < ctx->binding_count; ++i) {
        wh_source_binding_t *b = &ctx->bindings[i];
        if (b->source) {
            obs_source_remove_audio_capture_callback(b->source,
                                                     wh_source_audio_capture, b);
            obs_source_release(b->source);
            b->source = NULL;
        }
    }
    ctx->binding_count = 0;
}

static bool wh_output_start(void *data)
{
    wh_output_t *ctx = data;
    uint16_t port = (uint16_t)obs_data_get_int(obs_output_get_settings(ctx->output), "port");
    if (!wh_net_listen(port, &ctx->listener)) {
        blog(LOG_ERROR, "[wormhole] cannot listen on port %u", port);
        return false;
    }
    ctx->running = 1;
    memset(ctx->desc_sent, 0, sizeof(ctx->desc_sent));
    wh_init_track_names(ctx);
    {
        int tracks = (int)obs_data_get_int(obs_output_get_settings(ctx->output), "tracks");
        size_t mask = 0;
        int i;
        if (tracks < 1) tracks = 2;
        if (tracks > 6) tracks = 6;
        ctx->audio_tracks = tracks;
        for (i = 0; i < tracks; ++i) mask |= ((size_t)1 << i);
        obs_output_set_mixers(ctx->output, mask);
        blog(LOG_INFO, "[wormhole] audio tracks 1-%d enabled", tracks);
    }
    if (!wh_thread_create(&ctx->thread_handle, wh_output_thread, ctx)) {
        blog(LOG_ERROR, "[wormhole] cannot start worker thread");
        ctx->running = 0;
        wh_net_close(ctx->listener);
        ctx->listener = WH_INVALID_SOCKET;
        return false;
    }
    ctx->thread_valid = 1; /* stop() may now claim exactly one shutdown */
    /* OBS sets its internal "active" flag here, and raw callbacks begin on
     * OBS's own threads. Calling this from the worker thread raced with
     * shutdown and was the source of the disable-time crash. */
    obs_output_begin_data_capture(ctx->output, 0);
    blog(LOG_INFO, "[wormhole] listening on %u", port);
    return true;
}

static void wh_output_stop(void *data, uint64_t ts)
{
    (void)ts;
    wh_output_t *ctx = data;
    /* obs_output_release() calls this callback a second time from OBS's
     * destroy path. Joining a thread handle twice / double-closing sockets
     * crashed OBS when the user disabled the uplink. Only one caller may
     * perform the teardown. */
    if (!ctx->thread_valid)
        return;
    ctx->thread_valid = 0;
    blog(LOG_INFO, "[wormhole] output stop requested");
    ctx->running = 0;
    wh_net_close(ctx->listener);
    ctx->listener = WH_INVALID_SOCKET;
    wh_net_close(ctx->client);
    ctx->client = WH_INVALID_SOCKET;
    wh_thread_join(ctx->thread_handle);
    ctx->thread_handle = NULL;
    wh_output_drain(ctx);
    /* OBS does NOT end raw data capture in the normal obs_output_stop()
     * path - the output plugin must do it. Without this, the canvas keeps
     * its raw_active count and OBS grays out Settings > Output Mode forever.
     * obs_output_destroy() waits for the async end-capture thread. */
    obs_output_end_data_capture(ctx->output);
    wh_output_release_bindings(ctx);
    blog(LOG_INFO, "[wormhole] data capture ended");
}

/* ---------- full-state scene manifest (pushed periodically) ---------- */
static void wh_appendf(char *buf, size_t cap, const char *fmt, ...)
{
    size_t at = strlen(buf);
    va_list ap;
    int n;
    if (at + 600u >= cap)
        return;
    va_start(ap, fmt);
    n = vsnprintf(buf + at, cap - at, fmt, ap);
    va_end(ap);
    if (n < 0)
        buf[at] = '\0';
}

static bool wh_manifest_item(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
    wh_manifest_ctx_t *m = param;
    obs_source_t *src = obs_sceneitem_get_source(item);
    const char *name;
    const char *kind;
    (void)scene;
    if (!src)
        return true;
    name = obs_source_get_name(src);
    if (!name)
        return true;
    if (m->collect) {
        if (obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO &&
            m->audio_count < 8) {
            snprintf(m->audio_names[m->audio_count], 128, "%s", name);
            ++m->audio_count;
        }
        return true;
    }
    kind = (obs_source_get_output_flags(src) & OBS_SOURCE_AUDIO) ? "audio" : "video";
    wh_appendf(m->buf, WH_MANIFEST_CAP, "I:%s|%s|%d|0|%d\n", name, kind,
               obs_sceneitem_visible(item) ? 1 : 0,
               obs_source_muted(src) ? 1 : 0);
    return true;
}

/* Runs on OBS's UI thread. Builds a complete, idempotent state snapshot:
 * V:version, A:active scene, T:id|name tracks, S:scene, I:name|kind|visible|track|muted. */
static void wh_build_manifest_task(void *param)
{
    wh_manifest_ctx_t *m = param;
    char *buf = m->buf;
    struct obs_frontend_source_list scenes = {0};
    obs_source_t *active;
    config_t *cfg;
    size_t i;
    int t;

    buf[0] = '\0';
    m->audio_count = 0;
    m->collect = 1;
    obs_frontend_get_scenes(&scenes);
    for (i = 0; i < scenes.sources.num; ++i) {
        obs_scene_t *scene = obs_scene_from_source(scenes.sources.array[i]);
        if (scene)
            obs_scene_enum_items(scene, wh_manifest_item, m);
    }
    m->collect = 0;

    wh_appendf(buf, WH_MANIFEST_CAP, "V:1\n");
    active = obs_frontend_get_current_scene();
    wh_appendf(buf, WH_MANIFEST_CAP, "A:%s\n", active ? obs_source_get_name(active) : "");
    if (active)
        obs_source_release(active);

    cfg = obs_frontend_get_profile_config();
    for (t = 1; t <= (m->track_count ? m->track_count : 2); ++t) {
        char key[32];
        const char *val = NULL;
        const char *fallback;
        snprintf(key, sizeof(key), "Track%dName", t);
        if (cfg)
            val = config_get_string(cfg, "AdvOut", key);
        fallback = (t - 1 < m->audio_count) ? m->audio_names[t - 1] : "";
        if (!fallback[0]) {
            char generated[32];
            snprintf(generated, sizeof(generated), "Track %d", t);
            wh_appendf(buf, WH_MANIFEST_CAP, "T:%d|%s\n", t, generated);
        } else {
            wh_appendf(buf, WH_MANIFEST_CAP, "T:%d|%s\n", t,
                       (val && *val) ? val : fallback);
        }
    }

    for (i = 0; i < scenes.sources.num; ++i) {
        obs_source_t *scene_src = scenes.sources.array[i];
        obs_scene_t *scene = obs_scene_from_source(scene_src);
        wh_appendf(buf, WH_MANIFEST_CAP, "S:%s\n", obs_source_get_name(scene_src));
        if (scene)
            obs_scene_enum_items(scene, wh_manifest_item, m);
    }
    obs_frontend_source_list_free(&scenes);
}

static void wh_send_manifest(wh_output_t *ctx)
{
    wh_manifest_ctx_t m;
    uint8_t *frame = malloc(4u * 1024u * 1024u);
    size_t n;
    memset(&m, 0, sizeof(m));
    m.track_count = ctx->audio_tracks;
    m.buf = bzalloc(WH_MANIFEST_CAP);
    if (!m.buf || !frame) {
        bfree(m.buf);
        free(frame);
        return;
    }
    obs_queue_task(OBS_TASK_UI, wh_build_manifest_task, &m, true);
    n = wh2_build_text(frame, 4u * 1024u * 1024u, WH2_MSG_SCENE_MANIFEST,
                       ++ctx->sequence, 0, m.buf);
    if (n) {
        wh_net_send(ctx->client, frame, n);
        snprintf(ctx->last_manifest, sizeof(ctx->last_manifest), "%s", m.buf);
        blog(LOG_INFO, "[wormhole] scene manifest sent (%zu bytes)", strlen(m.buf));
    }
    bfree(m.buf);
    free(frame);
}

static void wh_output_raw_video(void *data, struct video_data *frame)
{
    wh_output_t *ctx = data;
    static int logged_once = 0;
    if (!logged_once) {
        logged_once = 1;
        blog(LOG_INFO, "[wormhole] first raw video callback");
    }
    const struct video_output_info *info = video_output_get_info(obs_output_video(ctx->output));
    int width = (int)info->width;
    int height = (int)info->height;
    int subsampling = TJSAMP_420;
    size_t y_size;
    size_t c_size;
    size_t total;
    wh_video_item_t *item;
    uint8_t *dst;

    if (!ctx->running || ctx->video_count >= WH_MAX_QUEUE)
        return;
    if (info->format == VIDEO_FORMAT_I444)
        subsampling = TJSAMP_444;
    else if (info->format == VIDEO_FORMAT_I422)
        subsampling = TJSAMP_422;
    y_size = (size_t)width * height;
    c_size = (subsampling == TJSAMP_444) ? y_size
             : (subsampling == TJSAMP_422) ? y_size / 2 : y_size / 4;
    total = y_size + c_size * 2;

    wh_mutex_lock(&ctx->video_mutex);
    item = ctx->video_pool;
    if (item) {
        ctx->video_pool = item->pool_next;
        item->pool_next = NULL;
        if (item->yuv_len < total) {
            free(item->yuv);
            item->yuv = malloc(total);
            item->yuv_len = item->yuv ? total : 0;
        }
    } else {
        item = bzalloc(sizeof(*item));
        item->yuv = malloc(total);
        item->yuv_len = item->yuv ? total : 0;
    }
    wh_mutex_unlock(&ctx->video_mutex);
    if (!item || !item->yuv)
        return;

    dst = item->yuv;
    if (info->format == VIDEO_FORMAT_NV12) {
        size_t i;
        memcpy(dst, frame->data[0], y_size);
        for (i = 0; i < c_size; ++i) {
            dst[y_size + i] = frame->data[1][i * 2];
            dst[y_size + c_size + i] = frame->data[1][i * 2 + 1];
        }
    } else {
        memcpy(dst, frame->data[0], y_size);
        memcpy(dst + y_size, frame->data[1], c_size);
        memcpy(dst + y_size + c_size, frame->data[2], c_size);
    }
    item->width = width;
    item->height = height;
    item->subsampling = subsampling;
    item->timestamp = frame->timestamp;

    wh_mutex_lock(&ctx->video_mutex);
    item->next = NULL;
    if (ctx->video_tail) ctx->video_tail->next = item;
    else ctx->video_head = item;
    ctx->video_tail = item;
    ++ctx->video_count;
    wh_mutex_unlock(&ctx->video_mutex);
}

/* Authoritative audio path: one network channel per OBS output track. */
static void wh_output_raw_audio2(void *data, size_t idx, struct audio_data *frames)
{
    wh_output_t *ctx = data;
    wh_audio_item_t *item;
    const struct audio_output_info *ainfo;
    size_t bytes;
    size_t i;
    int c;

    if (!ctx->running || idx >= 6 || ctx->audio_count >= WH_MAX_AUDIO_QUEUE)
        return;
    bytes = (size_t)frames->frames * 2u * sizeof(int16_t);
    item = bzalloc(sizeof(*item));
    if (!item)
        return;
    item->pcm = malloc(bytes);
    if (!item->pcm) {
        bfree(item);
        return;
    }
    ainfo = audio_output_get_info(obs_output_audio(ctx->output));
    for (i = 0; i < frames->frames; ++i) {
        for (c = 0; c < 2; ++c) {
            float value = 0.0f;
            int sample;
            if (ainfo && ainfo->format == AUDIO_FORMAT_FLOAT_PLANAR)
                value = ((const float *)frames->data[c])[i];
            else if (ainfo && ainfo->format == AUDIO_FORMAT_FLOAT)
                value = ((const float *)frames->data[0])[i * 2u + (size_t)c];
            sample = (int)(value * 32767.0f);
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            ((int16_t *)item->pcm)[i * 2u + (size_t)c] = (int16_t)sample;
        }
    }
    item->pcm_len = bytes;
    item->track_id = (uint32_t)idx + 1u;
    item->frames = frames->frames;
    item->sample_rate = 48000;
    item->channels = 2;
    item->bits = 16;
    item->timestamp = frames->timestamp;
    snprintf(item->name, sizeof(item->name), "%s", ctx->track_names[idx]);
    item->need_desc = 0;
    if (!ctx->desc_sent[idx]) {
        item->need_desc = 1;
        ctx->desc_sent[idx] = 1;
    }
    if ((int)idx + 1 > ctx->track_count)
        ctx->track_count = (int)idx + 1;

    wh_mutex_lock(&ctx->audio_mutex);
    item->next = NULL;
    if (ctx->audio_tail) ctx->audio_tail->next = item;
    else ctx->audio_head = item;
    ctx->audio_tail = item;
    ++ctx->audio_count;
    wh_mutex_unlock(&ctx->audio_mutex);
}

static struct obs_output_info wh_output_info = {
    .id = "wormhole-output",
    /* Video is captured through OBS raw callbacks. Audio is captured per
     * source via obs_source_add_audio_capture_callback() instead, so each
     * source keeps its identity instead of being summed into one mixer. */
    .flags = OBS_OUTPUT_AV | OBS_OUTPUT_MULTI_TRACK,
    .get_name = wh_output_name,
    .create = wh_output_create,
    .destroy = wh_output_destroy,
    .get_defaults = wh_output_defaults,
    .get_properties = wh_output_properties,
    .start = wh_output_start,
    .stop = wh_output_stop,
    .raw_video = wh_output_raw_video,
    .raw_audio2 = wh_output_raw_audio2,
};

void wh_register_output(void)
{
    obs_register_output(&wh_output_info);
}

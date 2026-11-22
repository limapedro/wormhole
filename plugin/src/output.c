/* Wormhole sender: one composited video signal + N OBS output-track audio channels. */
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WH_MAX_QUEUE 120
#define WH_MAX_AUDIO_QUEUE 240
#define WH_FRAME_SIZE (4u * 1024u * 1024u)

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
    volatile int negotiated;
    wh_thread_t thread_handle;
    wh_thread_t audio_thread;
    volatile int audio_thread_valid;
    wh_mutex_t send_mutex;
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
    int tracks;
    char name[128];
    char track_names[6][128];
    int desc_sent[6];
    uint32_t sequence;
    uint64_t dropped;
} wh_output_t;

static const char *wh_output_name(void *type_data)
{
    (void)type_data;
    return "Wormhole Sender";
}

static void wh_output_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "port", 18090);
    obs_data_set_default_int(settings, "quality", 90);
    obs_data_set_default_int(settings, "tracks", 2);
}

static obs_properties_t *wh_output_properties(void *data)
{
    (void)data;
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_int(props, "port", "TCP Port", 1024, 65535, 1);
    obs_properties_add_int(props, "tracks", "Audio Tracks (1-6)", 1, 6, 1);
    obs_properties_add_int_slider(props, "quality", "JPEG Quality", 1, 100, 1);
    return props;
}

/* Track names: OBS's own AdvOut Track<N>Name values first, then the first
 * audio sources found, then "Track N". Called on OBS's UI thread. */
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

/* Dedicated audio sender: video/JPEG work must not delay audio packets. */
static WH_THREAD_API wh_audio_thread(WH_THREAD_ARG arg)
{
    wh_output_t *ctx = arg;
    uint8_t *frame = malloc(256u * 1024u);
    if (!frame)
        return WH_THREAD_RET;

    while (ctx->running) {
        wh_audio_item_t *audio = NULL;
        wh_socket_t client;
        size_t n = 0;

        if (!ctx->negotiated || ctx->client == WH_INVALID_SOCKET) {
            wh_sleep_ms(1);
            continue;
        }
        client = ctx->client;

        wh_mutex_lock(&ctx->audio_mutex);
        audio = ctx->audio_head;
        if (audio) {
            ctx->audio_head = audio->next;
            if (!ctx->audio_head)
                ctx->audio_tail = NULL;
            --ctx->audio_count;
        }
        wh_mutex_unlock(&ctx->audio_mutex);
        if (!audio) {
            wh_sleep_ms(1);
            continue;
        }

        if (audio->need_desc) {
            n = wh2_build_stream_desc(frame, 256u * 1024u, ++ctx->sequence,
                                      audio->timestamp, audio->track_id, audio->name,
                                      audio->sample_rate, audio->channels,
                                      audio->bits, 1, 0);
            wh_mutex_lock(&ctx->send_mutex);
            if (n && !wh_net_send(client, frame, n))
                ctx->running = 0;
            wh_mutex_unlock(&ctx->send_mutex);
            audio->need_desc = 0;
        }
        n = wh2_build_audio(frame, 256u * 1024u, ++ctx->sequence,
                            audio->timestamp, audio->track_id,
                            audio->frames, audio->sample_rate,
                            audio->channels, audio->bits,
                            audio->pcm, audio->pcm_len);
        wh_mutex_lock(&ctx->send_mutex);
        if (n && !wh_net_send(client, frame, n))
            ctx->running = 0;
        wh_mutex_unlock(&ctx->send_mutex);
        free(audio->pcm);
        bfree(audio);
    }
    free(frame);
    return WH_THREAD_RET;
}

/* Network worker: accept, negotiate, then send video + control frames. */
static WH_THREAD_API wh_output_thread(WH_THREAD_ARG arg)
{
    wh_output_t *ctx = arg;
    uint8_t *frame = malloc(WH_FRAME_SIZE);
    uint8_t *jpeg = NULL;
    wh_socket_t client = WH_INVALID_SOCKET;
    size_t frame_len = 0;
    wh2_hello_t hello;
    int negotiated = 0;

    if (!frame)
        return WH_THREAD_RET;
    memset(&hello, 0, sizeof(hello));

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
    ctx->client = client;

    if (!wh_net_recv_frame(client, frame, WH_FRAME_SIZE, &frame_len) ||
        wh2_parse_hello(frame, frame_len, &hello) != WH2_PARSE_OK) {
        blog(LOG_WARNING, "[wormhole] HELLO failed");
        free(frame);
        return WH_THREAD_RET;
    }
    {
        size_t n = wh2_build_hello_ack(frame, WH_FRAME_SIZE, ++ctx->sequence,
                                       0, WH2_CAP_MULTI_AUDIO, ctx->name);
        wh_mutex_lock(&ctx->send_mutex);
        if (n && wh_net_send(client, frame, n))
            negotiated = 1;
        wh_mutex_unlock(&ctx->send_mutex);
    }
    ctx->negotiated = negotiated;
    blog(LOG_INFO, "[wormhole] v2 negotiated, sending %d audio track(s)", ctx->tracks);

    while (ctx->running) {
        wh_video_item_t *video = NULL;
        size_t jpeg_len = 0;
        size_t n = 0;

        wh_mutex_lock(&ctx->video_mutex);
        video = ctx->video_head;
        if (video) {
            ctx->video_head = video->next;
            if (!ctx->video_head)
                ctx->video_tail = NULL;
            --ctx->video_count;
        }
        wh_mutex_unlock(&ctx->video_mutex);

        if (video) {
            if (wh_media_encode(&ctx->media, video->yuv, video->width,
                                video->height, video->subsampling,
                                ctx->quality, &jpeg, &jpeg_len)) {
                n = wh2_build_video(frame, WH_FRAME_SIZE, ++ctx->sequence,
                                    video->timestamp, video->timestamp,
                                    video->width, video->height, jpeg, jpeg_len);
                wh_mutex_lock(&ctx->send_mutex);
                if (n && !wh_net_send(client, frame, n))
                    ctx->running = 0;
                wh_mutex_unlock(&ctx->send_mutex);
                wh_media_free_jpeg(jpeg);
                jpeg = NULL;
            }
            wh_mutex_lock(&ctx->video_mutex);
            video->pool_next = ctx->video_pool;
            ctx->video_pool = video;
            wh_mutex_unlock(&ctx->video_mutex);
        } else {
            wh_sleep_ms(1);
        }
    }
    free(frame);
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
    ctx->listener = WH_INVALID_SOCKET;
    ctx->client = WH_INVALID_SOCKET;
    wh_mutex_init(&ctx->video_mutex);
    wh_mutex_init(&ctx->audio_mutex);
    wh_mutex_init(&ctx->send_mutex);
    wh_media_init(&ctx->media);
    return ctx;
}

static void wh_output_destroy(void *data)
{
    wh_output_t *ctx = data;
    if (!ctx)
        return;
    wh_media_free(&ctx->media);
    wh_mutex_free(&ctx->video_mutex);
    wh_mutex_free(&ctx->audio_mutex);
    wh_mutex_free(&ctx->send_mutex);
    bfree(ctx);
}

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

static bool wh_output_start(void *data)
{
    wh_output_t *ctx = data;
    uint16_t port = (uint16_t)obs_data_get_int(obs_output_get_settings(ctx->output), "port");

    if (!wh_net_listen(port, &ctx->listener)) {
        blog(LOG_ERROR, "[wormhole] cannot listen on port %u", port);
        return false;
    }
    ctx->running = 1;
    ctx->negotiated = 0;
    memset(ctx->desc_sent, 0, sizeof(ctx->desc_sent));

    if (!wh_thread_create(&ctx->thread_handle, wh_output_thread, ctx)) {
        blog(LOG_ERROR, "[wormhole] cannot start worker thread");
        ctx->running = 0;
        wh_net_close(ctx->listener);
        ctx->listener = WH_INVALID_SOCKET;
        return false;
    }
    ctx->thread_valid = 1;
    if (wh_thread_create(&ctx->audio_thread, wh_audio_thread, ctx))
        ctx->audio_thread_valid = 1;
    else
        blog(LOG_ERROR, "[wormhole] cannot start audio thread");

    obs_output_begin_data_capture(ctx->output, 0);
    blog(LOG_INFO, "[wormhole] listening on %u", port);
    return true;
}

static void wh_output_stop(void *data, uint64_t ts)
{
    (void)ts;
    wh_output_t *ctx = data;
    if (!ctx->thread_valid)
        return;
    ctx->thread_valid = 0;
    blog(LOG_INFO, "[wormhole] output stop requested");
    ctx->running = 0;
    ctx->negotiated = 0;
    wh_net_close(ctx->listener);
    ctx->listener = WH_INVALID_SOCKET;
    wh_net_close(ctx->client);
    ctx->client = WH_INVALID_SOCKET;
    wh_thread_join(ctx->thread_handle);
    ctx->thread_handle = NULL;
    if (ctx->audio_thread_valid) {
        wh_thread_join(ctx->audio_thread);
        ctx->audio_thread_valid = 0;
    }
    wh_output_drain(ctx);
    obs_output_end_data_capture(ctx->output);
    blog(LOG_INFO, "[wormhole] data capture ended");
}

static void wh_output_raw_video(void *data, struct video_data *frame)
{
    wh_output_t *ctx = data;
    const struct video_output_info *info;
    wh_video_item_t *item;
    int width, height, subsampling = TJSAMP_420;
    size_t y_size, c_size, total;
    uint8_t *dst;

    if (!ctx->running || ctx->video_count >= WH_MAX_QUEUE)
        return;
    info = video_output_get_info(obs_output_video(ctx->output));
    if (!info)
        return;
    width = (int)info->width;
    height = (int)info->height;
    if (info->format == VIDEO_FORMAT_I444)
        subsampling = TJSAMP_444;
    else if (info->format == VIDEO_FORMAT_I422)
        subsampling = TJSAMP_422;
    y_size = (size_t)width * height;
    c_size = (subsampling == TJSAMP_444) ? y_size
           : (subsampling == TJSAMP_422) ? y_size / 2 : y_size / 4;
    total = y_size + c_size * 2u;

    wh_mutex_lock(&ctx->video_mutex);
    item = ctx->video_pool;
    if (item)
        ctx->video_pool = item->pool_next;
    wh_mutex_unlock(&ctx->video_mutex);
    if (!item)
        item = bzalloc(sizeof(*item));
    if (!item)
        return;
    if (item->yuv_len < total) {
        free(item->yuv);
        item->yuv = malloc(total);
        item->yuv_len = total;
    }
    if (!item->yuv) {
        bfree(item);
        return;
    }
    item->width = width;
    item->height = height;
    item->subsampling = subsampling;
    item->timestamp = frame->timestamp;

    dst = item->yuv;
    /* OBS raw frames are commonly NV12 (2 planes, interleaved UV). Copy the
     * luma row-by-row so padded linesizes are safe, then deinterleave UV into
     * the planar Cb/Cr layout the JPEG encoder expects. NULL chroma planes
     * are treated as neutral 128 (this used to crash on NV12). */
    for (int row = 0; row < height; ++row)
        memcpy(dst + (size_t)row * width,
               frame->data[0] + (size_t)row * frame->linesize[0], (size_t)width);

    if (info->format == VIDEO_FORMAT_NV12 && frame->data[1]) {
        uint8_t *cb = dst + y_size;
        uint8_t *cr = cb + c_size;
        size_t cw = (size_t)width / 2;
        size_t ch = (size_t)height / 2;
        for (size_t row = 0; row < ch; ++row) {
            const uint8_t *src = frame->data[1] + row * frame->linesize[1];
            for (size_t col = 0; col < cw; ++col) {
                cb[row * cw + col] = src[col * 2];
                cr[row * cw + col] = src[col * 2 + 1];
            }
        }
    } else {
        if (frame->data[1])
            memcpy(dst + y_size, frame->data[1], c_size);
        else
            memset(dst + y_size, 128, c_size);
        if (frame->data[2])
            memcpy(dst + y_size + c_size, frame->data[2], c_size);
        else
            memset(dst + y_size + c_size, 128, c_size);
    }

    wh_mutex_lock(&ctx->video_mutex);
    item->next = NULL;
    if (ctx->video_tail) ctx->video_tail->next = item;
    else ctx->video_head = item;
    ctx->video_tail = item;
    ++ctx->video_count;
    wh_mutex_unlock(&ctx->video_mutex);
}

static struct obs_output_info wh_output_info = {
    .id = "wormhole-output",
    .flags = OBS_OUTPUT_VIDEO,
    .get_name = wh_output_name,
    .create = wh_output_create,
    .destroy = wh_output_destroy,
    .get_defaults = wh_output_defaults,
    .get_properties = wh_output_properties,
    .start = wh_output_start,
    .stop = wh_output_stop,
    .raw_video = wh_output_raw_video,
};

void wh_register_output(void)
{
    obs_register_output(&wh_output_info);
}

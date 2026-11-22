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
#include <turbojpeg.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WH_MAX_QUEUE 120
#define WH_MAX_AUDIO_QUEUE 240

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
} wh_audio_item_t;

typedef struct {
    obs_output_t *output;
    wh_media_t media;
    wh_socket_t listener;
    wh_socket_t client;
    volatile int running;
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
    int track_count;
    int desc_sent[16];
    uint32_t sequence;
    uint64_t dropped;
} wh_output_t;

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

static WH_THREAD_API wh_output_thread(WH_THREAD_ARG arg)
{
    wh_output_t *ctx = arg;
    uint8_t *frame = (uint8_t *)malloc(4u * 1024u * 1024u);
    size_t frame_len = 0;
    wh2_hello_t hello;
    wh2_hello_ack_t ack;
    int negotiated = 0;
    memset(&hello, 0, sizeof(hello));
    memset(&ack, 0, sizeof(ack));

    if (!wh_net_accept(ctx->listener, &ctx->client)) {
        blog(LOG_WARNING, "[wormhole] no sender connection");
        free(frame);
        return WH_THREAD_RET;
    }
    blog(LOG_INFO, "[wormhole] receiver connected");

    /* v2 negotiation: the receiver speaks first. */
    if (!wh_net_recv_frame(ctx->client, frame, 4u * 1024u * 1024u, &frame_len)) {
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
        int sent = n && wh_net_send(ctx->client, frame, n);
        blog(LOG_INFO, "[wormhole] ACK built=%zu sent=%d", n, sent);
        if (sent)
            negotiated = 1;
    }
    blog(LOG_INFO, "[wormhole] negotiation=%d, entering send loop", negotiated);
    if (!obs_output_active(ctx->output))
        obs_output_begin_data_capture(ctx->output, 0);
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
                if (n && !wh_net_send(ctx->client, frame, n))
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
                    char name[32];
                    snprintf(name, sizeof(name), "Track %u", (unsigned)audio->track_id);
                    n = wh2_build_stream_desc(frame, 4u * 1024u * 1024u, ++ctx->sequence,
                                              audio->timestamp, audio->track_id, name,
                                              audio->sample_rate, audio->channels,
                                              audio->bits, 1, 0);
                    if (!n || !wh_net_send(ctx->client, frame, n))
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
            if (n && !wh_net_send(ctx->client, frame, n))
                ctx->running = 0;
            free(audio->pcm);
            bfree(audio);
        }

        if (!video && !audio) {
            wh_sleep_ms(1);
        }
    }

    wh_net_close(ctx->client);
    ctx->client = WH_INVALID_SOCKET;
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
    wh_media_free(&ctx->media);
    wh_mutex_free(&ctx->video_mutex);
    wh_mutex_free(&ctx->audio_mutex);
    bfree(ctx);
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
#ifdef MAX_AUDIO_MIXES
    obs_output_set_mixers(ctx->output, (size_t)((1u << MAX_AUDIO_MIXES) - 1u));
#else
    obs_output_set_mixers(ctx->output, 0x3f);
#endif
    wh_thread_create(&ctx->thread_handle, wh_output_thread, ctx);
    blog(LOG_INFO, "[wormhole] listening on %u", port);
    return true;
}

static void wh_output_stop(void *data, uint64_t ts)
{
    (void)ts;
    wh_output_t *ctx = data;
    blog(LOG_INFO, "[wormhole] output stop requested");
    ctx->running = 0;
    wh_net_close(ctx->listener);
    ctx->listener = WH_INVALID_SOCKET;
    wh_net_close(ctx->client);
    ctx->client = WH_INVALID_SOCKET;
    wh_thread_join(ctx->thread_handle);
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

static void wh_output_raw_audio2(void *data, size_t idx, struct audio_data *frames)
{
    wh_output_t *ctx = data;
    wh_audio_item_t *item;
    const struct audio_output_info *ainfo;
    size_t bytes;
    size_t i;
    int c;

    if (!ctx->running || ctx->audio_count >= WH_MAX_AUDIO_QUEUE)
        return;
    bytes = (size_t)frames->frames * 2u * 2u;
    item = bzalloc(sizeof(*item));
    item->pcm = malloc(bytes);
    if (!item->pcm) { bfree(item); return; }

    /* OBS hands us planar 32-bit float by default. Convert exactly two
     * channels to interleaved S16 so the wire payload size is predictable. */
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
    item->need_desc = 0;
    if (idx >= 0 && idx < 16 && !ctx->desc_sent[idx]) {
        item->need_desc = 1;
        ctx->desc_sent[idx] = 1;
    }
    if ((size_t)idx + 1u > (size_t)ctx->track_count)
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

/* Wormhole receiver: one composited video signal. Audio channels are separate
 * wormhole-audio sources, each with its own connection (see audio.c). */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/channels.h"
#include "wormhole/discovery.h"
#include "wormhole/transport.h"
#include "wormhole/media.h"
#include "wormhole/protocol_v2.h"
#include "wormhole/queue.h"

#include <obs-module.h>
#include <turbojpeg.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WH_VIDEO_QUEUE 8

typedef struct wh_video_item {
    struct wh_video_item *next;
    uint8_t *data;
    size_t len;
    uint64_t timestamp;
    int width;
    int height;
} wh_video_item_t;

typedef struct {
    obs_source_t *source;
    wh_media_t media;
    wh_socket_t sock;
    volatile int running;
    wh_thread_t reader_thread;
    wh_thread_t video_thread;
    wh_mutex_t video_mutex;
    wh_video_item_t *video_head;
    wh_video_item_t *video_tail;
    int video_count;
    uint8_t frame[4u * 1024u * 1024u];
    uint8_t *yuv;
    size_t yuv_cap;
    struct obs_source_frame2 video_frame;
    int have_offset;
    uint64_t clock_offset;
    char host[128];
    uint16_t port;
    uint64_t frames;
    uint64_t dropped;
} wh_source_t;

static const char *wh_source_name(void *type_data)
{
    (void)type_data;
    return "Wormhole Video Capture";
}

static void wh_source_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "host", "127.0.0.1");
    obs_data_set_default_int(settings, "port", 18090);
}

static bool wh_scan_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
    (void)props; (void)p; (void)data;
    blog(LOG_INFO, "[wormhole] network scan requested");
    return true;
}

static obs_properties_t *wh_source_properties(void *data)
{
    obs_properties_t *props = obs_properties_create();
    obs_property_t *p;
    wh_peer_t found[WH_MAX_PEERS];
    size_t count = wh_discovery_peers(found, WH_MAX_PEERS);
    size_t i;

    (void)data;
    p = obs_properties_add_text(props, "banner",
        "  ______________________________________________________\n"
        " | W O R M H O L E  //  V I D E O   C A P T U R E          |\n"
        " |  gaming PC output video - add audio sources separately  |\n"
        " |____________________________________________________|\n",
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_WARNING);

    p = obs_properties_add_list(props, "peer", "> DISCOVERED GAME PCs",
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(p, "- MANUAL ENTRY -", "");
    for (i = 0; i < count; ++i) {
        char key[160];
        char label[256];
        snprintf(key, sizeof(key), "%s:%u", found[i].ip, (unsigned)found[i].port);
        snprintf(label, sizeof(label), "%s  /  %s", found[i].name, key);
        obs_property_list_add_string(p, label, key);
    }
    obs_properties_add_button2(props, "scan", "> REFRESH NETWORK LIST", wh_scan_clicked, NULL);
    obs_properties_add_text(props, "host", "> HOST / IP", OBS_TEXT_DEFAULT);
    obs_properties_add_int(props, "port", "> TCP PORT", 1024, 65535, 1);

    p = obs_properties_add_text(props, "info",
        "Video only. Add one or more 'Wormhole Audio' sources, each pointing at "
        "a 'Wormhole Audio Output' on the gaming PC, then assign OBS recording "
        "tracks in Advanced Audio Properties.", OBS_TEXT_INFO);
    return props;
}

static bool ensure_yuv(wh_source_t *ctx, int width, int height, int subsampling)
{
    size_t y = (size_t)width * height;
    size_t c = (subsampling == TJSAMP_444) ? y
             : (subsampling == TJSAMP_422) ? y / 2 : y / 4;
    size_t total = y + c * 2u;

    if (total > ctx->yuv_cap) {
        free(ctx->yuv);
        ctx->yuv = malloc(total);
        if (!ctx->yuv)
            return false;
        ctx->yuv_cap = total;
    }
    memset(&ctx->video_frame, 0, sizeof(ctx->video_frame));
    ctx->video_frame.format = (subsampling == TJSAMP_444) ? VIDEO_FORMAT_I444
                            : (subsampling == TJSAMP_422) ? VIDEO_FORMAT_I422
                            : VIDEO_FORMAT_I420;
    ctx->video_frame.width = (uint32_t)width;
    ctx->video_frame.height = (uint32_t)height;
    ctx->video_frame.data[0] = ctx->yuv;
    ctx->video_frame.data[1] = ctx->yuv + y;
    ctx->video_frame.data[2] = ctx->yuv + y + c;
    ctx->video_frame.linesize[0] = (uint32_t)width;
    ctx->video_frame.linesize[1] = (uint32_t)((subsampling == TJSAMP_444) ? width : width / 2);
    ctx->video_frame.linesize[2] = ctx->video_frame.linesize[1];
    ctx->video_frame.range = VIDEO_RANGE_PARTIAL;
    video_format_get_parameters(VIDEO_CS_709, VIDEO_RANGE_PARTIAL,
                                ctx->video_frame.color_matrix,
                                ctx->video_frame.color_range_min,
                                ctx->video_frame.color_range_max);
    return true;
}

static uint64_t wh_relative_ts(wh_source_t *ctx, uint64_t timestamp)
{
    if (!ctx->have_offset) {
        ctx->clock_offset = timestamp;
        ctx->have_offset = 1;
    }
    return timestamp - ctx->clock_offset;
}

static WH_THREAD_API wh_video_thread(WH_THREAD_ARG arg)
{
    wh_source_t *ctx = arg;
    for (;;) {
        wh_video_item_t *item = NULL;
        int w = 0, h = 0, sub = 0;

        wh_mutex_lock(&ctx->video_mutex);
        item = ctx->video_head;
        if (item) {
            ctx->video_head = item->next;
            if (!ctx->video_head)
                ctx->video_tail = NULL;
            --ctx->video_count;
        }
        wh_mutex_unlock(&ctx->video_mutex);
        if (!item) {
            if (!ctx->running)
                break;
            wh_sleep_ms(1);
            continue;
        }
        if (item->width >= 16 && item->height >= 16 &&
            item->width <= 4096 && item->height <= 4096 &&
            ensure_yuv(ctx, item->width, item->height, TJSAMP_420) &&
            wh_media_decode(&ctx->media, item->data, item->len,
                            ctx->yuv, &w, &h, &sub) &&
            w >= 16 && h >= 16 && w <= 4096 && h <= 4096 &&
            ensure_yuv(ctx, w, h, sub)) {
            ctx->video_frame.timestamp = wh_relative_ts(ctx, item->timestamp);
            obs_source_output_video2(ctx->source, &ctx->video_frame);
            ++ctx->frames;
        } else {
            ++ctx->dropped;
        }
        free(item->data);
        bfree(item);
    }
    return WH_THREAD_RET;
}

static void wh_refresh_props_task(void *param)
{
    obs_source_t *source = param;
    obs_source_update_properties(source);
    obs_source_release(source);
}

static WH_THREAD_API wh_reader_thread(WH_THREAD_ARG arg)
{
    wh_source_t *ctx = arg;
    size_t len = 0;
    uint8_t hello[1024];
    size_t hello_len;

    while (ctx->running && ctx->sock == WH_INVALID_SOCKET) {
        obs_data_t *settings = obs_source_get_settings(ctx->source);
        const char *peer = obs_data_get_string(settings, "peer");
        const char *manual = obs_data_get_string(settings, "host");
        uint16_t port = (uint16_t)obs_data_get_int(settings, "port");
        if (peer && *peer) {
            char ip[96] = "";
            unsigned p = 0;
            if (sscanf(peer, "%95[^:]:%u", ip, &p) == 2 && p) {
                snprintf(ctx->host, sizeof(ctx->host), "%s", ip);
                port = (uint16_t)p;
            }
        } else {
            snprintf(ctx->host, sizeof(ctx->host), "%s", manual ? manual : "127.0.0.1");
        }
        ctx->port = port ? port : 18090;
        obs_data_release(settings);
        if (wh_net_connect(ctx->host, ctx->port, &ctx->sock))
            break;
        wh_sleep_ms(500);
    }
    if (ctx->sock == WH_INVALID_SOCKET)
        return WH_THREAD_RET;
    blog(LOG_INFO, "[wormhole] video linked to %s:%u", ctx->host, ctx->port);

    hello_len = wh2_build_hello(hello, sizeof(hello), 1, 0,
                                WH2_CAP_CLOCK_SYNC, "wormhole-video",
                                3, 1920, 1080, 60, 0);
    if (!hello_len || !wh_net_send(ctx->sock, hello, hello_len))
        goto done;
    ctx->have_offset = 0;

    while (ctx->running &&
           wh_net_recv_frame(ctx->sock, ctx->frame, sizeof(ctx->frame), &len)) {
        wh2_header_t header;
        if (wh2_header_decode(ctx->frame, len, &header) != WH2_PARSE_OK)
            continue;
        if (header.type == WH2_MSG_HELLO_ACK) {
            wh2_hello_ack_t ack;
            if (wh2_parse_hello_ack(ctx->frame, len, &ack) == WH2_PARSE_OK)
                blog(LOG_INFO, "[wormhole] video negotiated with '%s'", ack.sender_name);
            continue;
        }
        if (header.type == WH2_MSG_AUDIO_CHANNELS) {
            char text[16 * 1024];
            if (wh2_parse_text(ctx->frame, len, text, sizeof(text))) {
                wh_channels_set(ctx->host, text);
                {
                    wh_channel_t list[16];
                    blog(LOG_INFO, "[wormhole] learned %zu remote audio channel(s)",
                         wh_channels_list(list, 16));
                    obs_source_t *src = obs_source_get_ref(ctx->source);
                    if (src)
                        obs_queue_task(OBS_TASK_UI, wh_refresh_props_task, src, false);
                }
            }
            continue;
        }
        if (header.type != WH2_MSG_VIDEO)
            continue;
        {
            wh2_video_t video;
            wh_video_item_t *item;
            if (wh2_parse_video(ctx->frame, len, &video) != WH2_PARSE_OK)
                continue;
            if (ctx->video_count >= WH_VIDEO_QUEUE) {
                ++ctx->dropped;
                continue;
            }
            item = bzalloc(sizeof(*item));
            if (!item)
                continue;
            item->data = malloc(video.payload_len);
            if (!item->data) {
                bfree(item);
                continue;
            }
            memcpy(item->data, video.payload, video.payload_len);
            item->len = video.payload_len;
            item->timestamp = header.timestamp_ns;
            item->width = (int)video.width;
            item->height = (int)video.height;
            wh_mutex_lock(&ctx->video_mutex);
            item->next = NULL;
            if (ctx->video_tail) ctx->video_tail->next = item;
            else ctx->video_head = item;
            ctx->video_tail = item;
            ++ctx->video_count;
            wh_mutex_unlock(&ctx->video_mutex);
        }
    }

done:
    wh_net_close(ctx->sock);
    ctx->sock = WH_INVALID_SOCKET;
    blog(LOG_INFO, "[wormhole] video link dropped: %llu frames, %llu dropped",
         (unsigned long long)ctx->frames, (unsigned long long)ctx->dropped);
    return WH_THREAD_RET;
}

/* Periodic reconnect loop: a dropped sender link recovers automatically. */
static WH_THREAD_API wh_source_thread(WH_THREAD_ARG arg)
{
    wh_source_t *ctx = arg;
    while (ctx->running) {
        wh_reader_thread(arg);
        if (!ctx->running)
            break;
        wh_sleep_ms(1000);
    }
    return WH_THREAD_RET;
}

static void *wh_source_create(obs_data_t *settings, obs_source_t *source)
{
    wh_source_t *ctx = bzalloc(sizeof(*ctx));
    const char *host = obs_data_get_string(settings, "host");
    ctx->source = source;
    snprintf(ctx->host, sizeof(ctx->host), "%s", host ? host : "127.0.0.1");
    ctx->port = (uint16_t)obs_data_get_int(settings, "port");
    if (!ctx->port)
        ctx->port = 18090;
    ctx->sock = WH_INVALID_SOCKET;
    ctx->yuv_cap = 16u * 1024u * 1024u;
    ctx->yuv = malloc(ctx->yuv_cap);
    wh_media_init(&ctx->media);
    wh_mutex_init(&ctx->video_mutex);
    wh_discovery_receiver_start();
    ctx->running = 1;
    wh_thread_create(&ctx->video_thread, wh_video_thread, ctx);
    wh_thread_create(&ctx->reader_thread, wh_source_thread, ctx);
    return ctx;
}

static void wh_source_destroy(void *data)
{
    wh_source_t *ctx = data;
    if (!ctx)
        return;
    ctx->running = 0;
    wh_net_close(ctx->sock);
    wh_thread_join(ctx->reader_thread);
    wh_thread_join(ctx->video_thread);
    wh_discovery_receiver_stop();
    wh_mutex_lock(&ctx->video_mutex);
    while (ctx->video_head) {
        wh_video_item_t *v = ctx->video_head;
        ctx->video_head = v->next;
        free(v->data);
        bfree(v);
    }
    wh_mutex_unlock(&ctx->video_mutex);
    wh_mutex_free(&ctx->video_mutex);
    wh_media_free(&ctx->media);
    free(ctx->yuv);
    bfree(ctx);
}

static void wh_source_activate(void *data)
{
    (void)data;
}

static struct obs_source_info wh_source_info = {
    .id = "wormhole-source",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_DO_NOT_DUPLICATE,
    .get_name = wh_source_name,
    .create = wh_source_create,
    .destroy = wh_source_destroy,
    .get_defaults = wh_source_defaults,
    .get_properties = wh_source_properties,
    .activate = wh_source_activate,
};

void wh_register_source(void)
{
    obs_register_source(&wh_source_info);
}

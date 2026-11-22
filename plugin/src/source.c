/* Wormhole OBS source: receiver with v2 negotiation and multi-track decoding. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/discovery.h"
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

typedef struct {
    obs_source_t *source;
    wh_media_t media;
    wh_socket_t sock;
    volatile int running;
    wh_thread_t thread;
    uint8_t frame[4 * 1024 * 1024];
    uint8_t *yuv;
    size_t yuv_cap;
    struct obs_source_frame2 video_frame;
    struct obs_source_audio audio_frame;
    int v2;
    int track_count;
    uint64_t frames;
    uint64_t audio_frames;
    uint64_t clock_offset;
    int have_offset;
    char host[128];
    uint16_t port;
    char peer_name[128];
} wh_source_t;

static const char *wh_source_name(void *type_data)
{
    (void)type_data;
    return "WORMHOLE // RECEIVER";
}

static void wh_source_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "host", "127.0.0.1");
    obs_data_set_default_int(settings, "port", 18090);
}

static bool wh_scan_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
    (void)props; (void)p; (void)data;
    wh_peer_t found[WH_MAX_PEERS];
    size_t count = wh_discovery_peers(found, WH_MAX_PEERS);
    size_t i;
    blog(LOG_INFO, "[wormhole] network scan: %zu node(s) visible", count);
    for (i = 0; i < count; ++i)
        blog(LOG_INFO, "[wormhole]   [%zu] %s  %s:%u", i + 1, found[i].name,
             found[i].ip, (unsigned)found[i].port);
    return true; /* rebuild the dialog so the peer list refreshes */
}

static obs_properties_t *wh_source_properties(void *data)
{
    (void)data;
    obs_properties_t *props = obs_properties_create();
    obs_property_t *p;
    wh_peer_t found[WH_MAX_PEERS];
    size_t count = wh_discovery_peers(found, WH_MAX_PEERS);
    size_t i;

    p = obs_properties_add_text(props, "banner",
        "  ______________________________________________________\n"
        " | W O R M H O L E  //  S I G N A L  R E C E I V E R     |\n"
        " | v0.2.0  ::  auto-discovery + manual uplink           |\n"
        " |____________________________________________________|\n",
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_WARNING);

    p = obs_properties_add_text(props, "sec1",
        " ──[ 01 // DISCOVERED NODES ]──────────────────────────", OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_NORMAL);

    p = obs_properties_add_list(props, "peer", "> AUTO-DISCOVERED NODES",
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(p, "- SELECT / MANUAL ENTRY -", "");
    for (i = 0; i < count; ++i) {
        char key[128];
        char label[256];
        snprintf(key, sizeof(key), "%s:%u", found[i].ip, (unsigned)found[i].port);
        snprintf(label, sizeof(label), "%s  /  %s", found[i].name, key);
        obs_property_list_add_string(p, label, key);
    }
    obs_property_set_long_description(p,
        "Nodes announcing Wormhole on UDP 18092. Select one to link automatically.");
    obs_properties_add_button2(props, "scan", "> SCAN NETWORK", wh_scan_clicked, NULL);

    p = obs_properties_add_text(props, "sec2",
        " ──[ 02 // MANUAL LINK ]───────────────────────────────", OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_NORMAL);
    obs_properties_add_text(props, "host", "> HOST / IP", OBS_TEXT_DEFAULT);
    obs_properties_add_int(props, "port", "> TCP PORT", 1024, 65535, 1);

    p = obs_properties_add_text(props, "footer",
        " [ SELECT A NODE OR ENTER IP:PORT ]   [ CYBERPUNK BUILD ]",
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_WARNING);
    return props;
}

static int ensure_yuv(wh_source_t *ctx, int width, int height, int subsampling)
{
    size_t y = (size_t)width * height;
    size_t c = (subsampling == TJSAMP_444) ? y
             : (subsampling == TJSAMP_422) ? y / 2 : y / 4;
    size_t need = y + c * 2;
    if (need > ctx->yuv_cap) {
        uint8_t *grown = realloc(ctx->yuv, need);
        if (!grown) return 0;
        ctx->yuv = grown;
        ctx->yuv_cap = need;
    }
    memset(&ctx->video_frame, 0, sizeof(ctx->video_frame));
    ctx->video_frame.width = (uint32_t)width;
    ctx->video_frame.height = (uint32_t)height;
    ctx->video_frame.format = (subsampling == TJSAMP_444) ? VIDEO_FORMAT_I444
                            : (subsampling == TJSAMP_422) ? VIDEO_FORMAT_I422
                            : VIDEO_FORMAT_I420;
    ctx->video_frame.data[0] = ctx->yuv;
    ctx->video_frame.data[1] = ctx->yuv + y;
    ctx->video_frame.data[2] = ctx->yuv + y + c;
    ctx->video_frame.linesize[0] = (uint32_t)width;
    ctx->video_frame.linesize[1] = (uint32_t)((subsampling == TJSAMP_444) ? width : width / 2);
    ctx->video_frame.linesize[2] = ctx->video_frame.linesize[1];
    /* Use libobs's own conversion parameters, same as the reference path. */
    ctx->video_frame.range = VIDEO_RANGE_PARTIAL;
    video_format_get_parameters(VIDEO_CS_709, VIDEO_RANGE_PARTIAL,
                                ctx->video_frame.color_matrix,
                                ctx->video_frame.color_range_min,
                                ctx->video_frame.color_range_max);
    return 1;
}

static uint64_t wh_relative_ts(wh_source_t *ctx, uint64_t timestamp)
{
    if (!ctx->have_offset) {
        ctx->clock_offset = timestamp;
        ctx->have_offset = 1;
    }
    return timestamp - ctx->clock_offset;
}

/* One connection session. Returns when the link drops; the caller retries. */
static void wh_source_session(wh_source_t *ctx)
{
    size_t len = 0;
    uint8_t hello[1024];
    size_t hello_len;

    ctx->have_offset = 0; /* each sender stream has its own clock origin */
    while (ctx->running && ctx->sock == WH_INVALID_SOCKET) {
        obs_data_t *settings = obs_source_get_settings(ctx->source);
        const char *peer = obs_data_get_string(settings, "peer");
        const char *manual = obs_data_get_string(settings, "host");
        uint16_t port = (uint16_t)obs_data_get_int(settings, "port");
        if (peer && *peer) {
            char ip[64] = "";
            unsigned p = 0;
            if (sscanf(peer, "%63[^:]:%u", ip, &p) == 2 && p) {
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
        return;
    blog(LOG_INFO, "[wormhole] linking to %s:%u", ctx->host, ctx->port);
    hello_len = wh2_build_hello(hello, sizeof(hello), 1, 0,
                                WH2_CAP_MULTI_AUDIO | WH2_CAP_CLOCK_SYNC,
                                "wormhole-receiver", 3, 1920, 1080, 60, 1);
    if (!hello_len || !wh_net_send(ctx->sock, hello, hello_len)) {
        wh_net_close(ctx->sock);
        ctx->sock = WH_INVALID_SOCKET;
        return;
    }

    while (ctx->running && wh_net_recv_frame(ctx->sock, ctx->frame, 4u * 1024u * 1024u, &len)) {
        wh2_header_t header;
        blog(LOG_INFO, "[wormhole] recv frame %zu bytes magic=%.4s", len, (const char *)ctx->frame);
        if (wh2_header_decode(ctx->frame, len, &header) == WH2_PARSE_OK) {
            if (header.type == WH2_MSG_HELLO_ACK) {
                wh2_hello_ack_t ack;
                if (wh2_parse_hello_ack(ctx->frame, len, &ack) == WH2_PARSE_OK) {
                    ctx->v2 = 1;
                    snprintf(ctx->peer_name, sizeof(ctx->peer_name), "%s",
                             ack.sender_name[0] ? ack.sender_name : "unknown");
                    blog(LOG_INFO, "[wormhole] v2 linked to sender '%s', capabilities=0x%x",
                         ctx->peer_name, ack.capabilities);
                }
                continue;
            }
            if (header.type == WH2_MSG_STREAM_DESC) {
                wh2_stream_desc_t desc;
                if (wh2_parse_stream_desc(ctx->frame, len, &desc) == WH2_PARSE_OK) {
                    if (desc.track_id > (uint32_t)ctx->track_count)
                        ctx->track_count = (int)desc.track_id;
                    blog(LOG_INFO, "[wormhole] track %u: '%s' %u Hz, %u ch, %u-bit, clock=%u",
                         desc.track_id, desc.name, desc.sample_rate, desc.channels,
                         desc.bits, desc.clock_id);
                }
                continue;
            }
            if (header.type == WH2_MSG_VIDEO) {
                wh2_video_t video;
                int w = 0, h = 0, sub = 0;
                if (wh2_parse_video(ctx->frame, len, &video) != WH2_PARSE_OK)
                    continue;
                if (!ensure_yuv(ctx, (int)video.width, (int)video.height, TJSAMP_420))
                    continue;
                if (!wh_media_decode(&ctx->media, video.payload, video.payload_len,
                                     ctx->yuv, &w, &h, &sub)) {
                    if (++ctx->audio_frames % 120u == 0u)
                        blog(LOG_WARNING, "[wormhole] JPEG decode failed repeatedly");
                    continue;
                }
                if (!ensure_yuv(ctx, w, h, sub))
                    continue;
                ctx->video_frame.timestamp = wh_relative_ts(ctx, header.timestamp_ns);
                obs_source_output_video2(ctx->source, &ctx->video_frame);
                ++ctx->frames;
                if (ctx->frames % 120u == 0u)
                    blog(LOG_INFO, "[wormhole] video frames=%llu audio=%llu last=%dx%d sub=%d",
                         (unsigned long long)ctx->frames,
                         (unsigned long long)ctx->audio_frames, w, h, sub);
                continue;
            }
            if (header.type == WH2_MSG_AUDIO) {
                wh2_audio_t audio;
                if (wh2_parse_audio(ctx->frame, len, &audio) != WH2_PARSE_OK)
                    continue;
                memset(&ctx->audio_frame, 0, sizeof(ctx->audio_frame));
                ctx->audio_frame.data[0] = audio.payload;
                ctx->audio_frame.frames = audio.frames;
                ctx->audio_frame.samples_per_sec = audio.sample_rate;
                ctx->audio_frame.speakers = (audio.channels == 1) ? SPEAKERS_MONO : SPEAKERS_STEREO;
                ctx->audio_frame.format = AUDIO_FORMAT_16BIT;
                ctx->audio_frame.timestamp = wh_relative_ts(ctx, header.timestamp_ns);
                if (audio.track_id == 1)
                    obs_source_output_audio(ctx->source, &ctx->audio_frame);
                ++ctx->audio_frames;
                continue;
            }
            continue;
        }

        /* Teleport compatibility frame. */
        {
            wh_header_t th;
            wh_packet_t packet;
            size_t offset = 0;
            if (wh_decode(ctx->frame, len, &offset, &packet, WH_DEFAULT_MAX_PAYLOAD) != WH_OK)
                continue;
            th = packet.header;
            if (th.kind == WH_KIND_JPEG) {
                int w = 0, h = 0, sub = 0;
                if (!wh_media_decode(&ctx->media, packet.payload, packet.payload_len,
                                     ctx->yuv, &w, &h, &sub))
                    continue;
                if (!ensure_yuv(ctx, w, h, sub))
                    continue;
                ctx->video_frame.timestamp = wh_relative_ts(ctx, (uint64_t)th.timestamp);
                obs_source_output_video2(ctx->source, &ctx->video_frame);
                ++ctx->frames;
            }
            wh_packet_free(&packet);
        }
    }

    wh_net_close(ctx->sock);
    ctx->sock = WH_INVALID_SOCKET;
    blog(LOG_INFO, "[wormhole] link dropped: %llu video, %llu audio frames; retrying",
         (unsigned long long)ctx->frames, (unsigned long long)ctx->audio_frames);
}

/* Keep re-establishing the uplink. This lets the receiver recover when the
 * gaming PC disables and re-enables Wormhole, or restarts OBS entirely. */
static WH_THREAD_API wh_source_thread(WH_THREAD_ARG arg)
{
    wh_source_t *ctx = arg;
    while (ctx->running) {
        wh_source_session(ctx);
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
    if (!ctx->port) ctx->port = 18090;
    ctx->sock = WH_INVALID_SOCKET;
    ctx->yuv_cap = 16u * 1024u * 1024u;
    ctx->yuv = (uint8_t *)malloc(ctx->yuv_cap);
    wh_media_init(&ctx->media);
    wh_discovery_receiver_start();
    ctx->running = 1;
    wh_thread_create(&ctx->thread, wh_source_thread, ctx);
    return ctx;
}

static void wh_source_destroy(void *data)
{
    wh_source_t *ctx = data;
    if (!ctx) return;
    ctx->running = 0;
    wh_net_close(ctx->sock);
    wh_thread_join(ctx->thread);
    wh_discovery_receiver_stop();
    wh_media_free(&ctx->media);
    free(ctx->yuv);
    bfree(ctx);
}

static void wh_source_activate(void *data)
{
    (void)data; /* video is pushed by the worker thread */
}

static struct obs_source_info wh_source_info = {
    .id = "wormhole-source",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE,
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

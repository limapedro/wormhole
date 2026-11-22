/* Wormhole per-channel audio: one source, one thread, one TCP connection each.
 *
 *   wormhole-audio-output  (gaming PC)  captures one chosen OBS audio source
 *   wormhole-audio         (streaming PC) receives one network audio channel
 *
 * Video remains a single composited stream on the main Wormhole port.
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/channels.h"
#include "wormhole/discovery.h"
#include "wormhole/transport.h"
#include "wormhole/protocol_v2.h"
#include "wormhole/queue.h"

#include <obs-module.h>
#include <obs-frontend-api.h>
#include <util/platform.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WH_AUDIO_QUEUE 240

typedef struct wh_audio_item {
    struct wh_audio_item *next;
    uint8_t *pcm;
    size_t len;
    uint32_t frames;
    uint64_t timestamp;
} wh_audio_item_t;

/* ---------------- sender: wormhole-audio-output ---------------- */

typedef struct {
    obs_source_t *source;
    obs_source_t *capture;
    wh_socket_t listener;
    wh_socket_t client;
    volatile int running;
    wh_thread_t thread;
    wh_mutex_t mutex;
    wh_audio_item_t *head;
    wh_audio_item_t *tail;
    int count;
    char name[128];
    uint16_t port;
    int bound;
} wh_audio_out_t;

static bool wh_enum_audio_sources(void *param, obs_source_t *source)
{
    obs_property_t *list = param;
    if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
        return true;
    obs_property_list_add_string(list, obs_source_get_name(source),
                                 obs_source_get_name(source));
    return true;
}

static const char *wh_audio_out_name(void *data)
{
    (void)data;
    return "WORMHOLE SEND - Gaming PC";
}

static void wh_audio_out_defaults(obs_data_t *settings)
{
    obs_data_set_default_int(settings, "port", 18091);
    obs_data_set_default_string(settings, "target", "");
}

static obs_properties_t *wh_audio_out_properties(void *data)
{
    obs_properties_t *props = obs_properties_create();
    obs_property_t *p;
    (void)data;
    p = obs_properties_add_text(props, "hint",
        "Sends one gaming-PC audio source to the streaming PC. Wormhole creates "
        "these automatically for every source in your Audio Mixer.", OBS_TEXT_INFO);
    p = obs_properties_add_list(props, "target", "> AUDIO SOURCE",
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(p, "- none -", "");
    obs_enum_sources(wh_enum_audio_sources, p);
    obs_properties_add_int(props, "port", "> TCP PORT", 1024, 65535, 1);
    return props;
}

static void wh_audio_out_capture(void *param, obs_source_t *source,
                                 const struct audio_data *audio_data, bool muted)
{
    wh_audio_out_t *ctx = param;
    wh_audio_item_t *item;
    size_t i;
    int c;
    (void)source; (void)muted;

    if (!ctx->running || ctx->count >= WH_AUDIO_QUEUE || !audio_data->frames)
        return;
    item = bzalloc(sizeof(*item));
    if (!item)
        return;
    item->len = (size_t)audio_data->frames * 2u * sizeof(int16_t);
    item->pcm = malloc(item->len);
    if (!item->pcm) {
        bfree(item);
        return;
    }
    for (i = 0; i < audio_data->frames; ++i) {
        for (c = 0; c < 2; ++c) {
            float v = audio_data->data[c ? 1 : 0] ?
                      ((const float *)audio_data->data[c ? 1 : 0])[i] : 0.0f;
            int s = (int)(v * 32767.0f);
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            ((int16_t *)item->pcm)[i * 2u + (size_t)c] = (int16_t)s;
        }
    }
    item->frames = audio_data->frames;
    item->timestamp = audio_data->timestamp;

    wh_mutex_lock(&ctx->mutex);
    item->next = NULL;
    if (ctx->tail) ctx->tail->next = item;
    else ctx->head = item;
    ctx->tail = item;
    ++ctx->count;
    wh_mutex_unlock(&ctx->mutex);
}

static WH_THREAD_API wh_audio_out_thread(WH_THREAD_ARG arg)
{
    wh_audio_out_t *ctx = arg;
    uint8_t *frame = malloc(256u * 1024u);
    wh_socket_t client = WH_INVALID_SOCKET;
    size_t n;

    if (!frame)
        return WH_THREAD_RET;
    if (!wh_net_accept(ctx->listener, &client)) {
        free(frame);
        return WH_THREAD_RET;
    }
    n = wh2_build_stream_desc(frame, 256u * 1024u, 1, 0, 1, ctx->name,
                              48000, 2, 16, 1, 0);
    if (!n || !wh_net_send(client, frame, n)) {
        wh_net_close(client);
        free(frame);
        return WH_THREAD_RET;
    }
    blog(LOG_INFO, "[wormhole] audio output '%s' connected", ctx->name);

    while (ctx->running) {
        wh_audio_item_t *item = NULL;
        wh_mutex_lock(&ctx->mutex);
        item = ctx->head;
        if (item) {
            ctx->head = item->next;
            if (!ctx->head) ctx->tail = NULL;
            --ctx->count;
        }
        wh_mutex_unlock(&ctx->mutex);
        if (!item) {
            wh_sleep_ms(1);
            continue;
        }
        n = wh2_build_audio(frame, 256u * 1024u, 2, item->timestamp, 1,
                            item->frames, 48000, 2, 16, item->pcm, item->len);
        if (!n || !wh_net_send(client, frame, n))
            ctx->running = 0;
        free(item->pcm);
        bfree(item);
    }
    wh_net_close(client);
    free(frame);
    return WH_THREAD_RET;
}

static void wh_audio_out_stop(wh_audio_out_t *ctx)
{
    if (ctx->bound && ctx->capture) {
        obs_source_remove_audio_capture_callback(ctx->capture,
                                                 wh_audio_out_capture, ctx);
        obs_source_release(ctx->capture);
        ctx->capture = NULL;
        ctx->bound = 0;
    }
    ctx->running = 0;
    wh_net_close(ctx->listener);
    ctx->listener = WH_INVALID_SOCKET;
    wh_net_close(ctx->client);
    ctx->client = WH_INVALID_SOCKET;
    wh_thread_join(ctx->thread);
    wh_mutex_lock(&ctx->mutex);
    while (ctx->head) {
        wh_audio_item_t *a = ctx->head;
        ctx->head = a->next;
        free(a->pcm);
        bfree(a);
    }
    ctx->tail = NULL;
    ctx->count = 0;
    wh_mutex_unlock(&ctx->mutex);
}

static void wh_audio_out_start(wh_audio_out_t *ctx, obs_data_t *settings)
{
    const char *target = obs_data_get_string(settings, "target");
    uint16_t port = (uint16_t)obs_data_get_int(settings, "port");
    obs_source_t *src;

    if (!target || !*target)
        return;
    src = obs_get_source_by_name(target);
    if (!src)
        return;
    ctx->capture = src;
    snprintf(ctx->name, sizeof(ctx->name), "%s", target);
    ctx->port = port;

    if (!wh_net_listen(port, &ctx->listener)) {
        blog(LOG_ERROR, "[wormhole] audio output port %u busy", port);
        obs_source_release(src);
        ctx->capture = NULL;
        return;
    }
    ctx->bound = 1;
    obs_source_add_audio_capture_callback(ctx->capture, wh_audio_out_capture, ctx);
    ctx->running = 1;
    wh_thread_create(&ctx->thread, wh_audio_out_thread, ctx);
    blog(LOG_INFO, "[wormhole] audio output '%s' listening on %u", ctx->name, port);
}

static void *wh_audio_out_create(obs_data_t *settings, obs_source_t *source)
{
    wh_audio_out_t *ctx = bzalloc(sizeof(*ctx));
    ctx->source = source;
    ctx->listener = WH_INVALID_SOCKET;
    ctx->client = WH_INVALID_SOCKET;
    wh_mutex_init(&ctx->mutex);
    if (obs_data_get_bool(settings, "connected"))
        wh_audio_out_start(ctx, settings);
    return ctx;
}

static void wh_audio_out_destroy(void *data)
{
    wh_audio_out_t *ctx = data;
    if (!ctx)
        return;
    wh_audio_out_stop(ctx);
    wh_mutex_free(&ctx->mutex);
    bfree(ctx);
}

static void wh_audio_out_update(void *data, obs_data_t *settings)
{
    wh_audio_out_t *ctx = data;
    wh_audio_out_stop(ctx);
    if (obs_data_get_bool(settings, "connected"))
        wh_audio_out_start(ctx, settings);
    else
        blog(LOG_INFO, "[wormhole] audio channel '%s' disconnected",
             obs_data_get_string(settings, "target"));
}

static struct obs_source_info wh_audio_out_info = {
    .id = "wormhole-audio-output",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = 0,
    .get_name = wh_audio_out_name,
    .create = wh_audio_out_create,
    .destroy = wh_audio_out_destroy,
    .update = wh_audio_out_update,
    .get_defaults = wh_audio_out_defaults,
    .get_properties = wh_audio_out_properties,
};

/* ---------------- receiver: wormhole-audio ---------------- */

typedef struct {
    obs_source_t *source;
    wh_socket_t sock;
    volatile int running;
    wh_thread_t thread;
    char host[128];
    uint16_t port;
    char name[128];
    struct obs_source_audio audio;
    uint8_t frame[256u * 1024u];
} wh_audio_in_t;

static const char *wh_audio_in_name(void *data)
{
    wh_audio_in_t *ctx = data;
    return ctx && ctx->name[0] ? ctx->name : "Wormhole Audio";
}

static void wh_audio_in_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "host", "127.0.0.1");
    obs_data_set_default_int(settings, "port", 18091);
}

/* Selecting a channel fills in host/port from the video connection. */
static bool wh_audio_channel_changed(void *priv, obs_properties_t *props,
                                     obs_property_t *property, obs_data_t *settings)
{
    const char *value = obs_data_get_string(settings, "channel");
    (void)priv; (void)property;
    if (value && *value) {
        char host[96] = "";
        unsigned port = 0;
        if (sscanf(value, "%95[^|]|%u", host, &port) == 2 && port) {
            obs_data_set_string(settings, "host", host);
            obs_data_set_int(settings, "port", (int)port);
        }
    }
    obs_property_set_visible(obs_properties_get(props, "host"), false);
    obs_property_set_visible(obs_properties_get(props, "port"), false);
    return true;
}

static obs_properties_t *wh_audio_in_properties(void *data)
{
    obs_properties_t *props = obs_properties_create();
    obs_property_t *p;
    wh_channel_t channels[16];
    size_t count = wh_channels_list(channels, 16);
    size_t i;

    (void)data;
    p = obs_properties_add_text(props, "hint",
        "Pick a gaming-PC audio source (microphone or output). Host and port "
        "are filled in automatically.", OBS_TEXT_INFO);
    p = obs_properties_add_list(props, "channel", "> GAMING PC AUDIO SOURCE",
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(p, "- MANUAL ENTRY -", "");
    for (i = 0; i < count; ++i) {
        char key[192];
        char label[256];
        snprintf(key, sizeof(key), "%s|%u", channels[i].host, (unsigned)channels[i].port);
        snprintf(label, sizeof(label), "%s  (port %u)", channels[i].name,
                 (unsigned)channels[i].port);
        obs_property_list_add_string(p, label, key);
    }
    obs_property_set_modified_callback2(p, wh_audio_channel_changed, NULL);
    obs_properties_add_text(props, "host", "> HOST / IP (auto)", OBS_TEXT_DEFAULT);
    obs_properties_add_int(props, "port", "> TCP PORT (auto)", 1024, 65535, 1);
    return props;
}

static WH_THREAD_API wh_audio_in_reader(WH_THREAD_ARG arg)
{
    wh_audio_in_t *ctx = arg;
    size_t len = 0;

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
        ctx->port = port ? port : 18091;
        obs_data_release(settings);
        if (wh_net_connect(ctx->host, ctx->port, &ctx->sock))
            break;
        wh_sleep_ms(500);
    }
    if (ctx->sock == WH_INVALID_SOCKET)
        return WH_THREAD_RET;

    while (ctx->running &&
           wh_net_recv_frame(ctx->sock, ctx->frame, sizeof(ctx->frame), &len)) {
        wh2_header_t header;
        if (wh2_header_decode(ctx->frame, len, &header) != WH2_PARSE_OK)
            continue;
        if (header.type == WH2_MSG_STREAM_DESC) {
            wh2_stream_desc_t desc;
            if (wh2_parse_stream_desc(ctx->frame, len, &desc) == WH2_PARSE_OK) {
                snprintf(ctx->name, sizeof(ctx->name), "%s", desc.name);
                blog(LOG_INFO, "[wormhole] audio input name: %s", ctx->name);
            }
            continue;
        }
        if (header.type == WH2_MSG_AUDIO) {
            wh2_audio_t audio;
            if (wh2_parse_audio(ctx->frame, len, &audio) != WH2_PARSE_OK)
                continue;
            memset(&ctx->audio, 0, sizeof(ctx->audio));
            ctx->audio.data[0] = audio.payload;
            ctx->audio.frames = audio.frames;
            ctx->audio.samples_per_sec = audio.sample_rate;
            ctx->audio.speakers = (audio.channels == 1) ? SPEAKERS_MONO : SPEAKERS_STEREO;
            ctx->audio.format = AUDIO_FORMAT_16BIT;
            ctx->audio.timestamp = os_gettime_ns();
            obs_source_output_audio(ctx->source, &ctx->audio);
        }
    }
    wh_net_close(ctx->sock);
    ctx->sock = WH_INVALID_SOCKET;
    return WH_THREAD_RET;
}

/* Reconnect loop: settings changes or link loss re-open the connection. */
static WH_THREAD_API wh_audio_in_thread(WH_THREAD_ARG arg)
{
    wh_audio_in_t *ctx = arg;
    while (ctx->running) {
        wh_audio_in_reader(arg);
        if (!ctx->running)
            break;
        wh_sleep_ms(1000);
    }
    return WH_THREAD_RET;
}

static void wh_audio_in_update(void *data, obs_data_t *settings)
{
    wh_audio_in_t *ctx = data;
    (void)settings;
    if (!ctx)
        return;
    wh_net_close(ctx->sock);
    ctx->sock = WH_INVALID_SOCKET;
    blog(LOG_INFO, "[wormhole] audio input reconnecting");
}

static void *wh_audio_in_create(obs_data_t *settings, obs_source_t *source)
{
    wh_audio_in_t *ctx = bzalloc(sizeof(*ctx));
    (void)settings;
    ctx->source = source;
    ctx->sock = WH_INVALID_SOCKET;
    ctx->running = 1;
    wh_thread_create(&ctx->thread, wh_audio_in_thread, ctx);
    return ctx;
}

static void wh_audio_in_destroy(void *data)
{
    wh_audio_in_t *ctx = data;
    if (!ctx)
        return;
    ctx->running = 0;
    wh_net_close(ctx->sock);
    wh_thread_join(ctx->thread);
    bfree(ctx);
}

static struct obs_source_info wh_audio_in_info = {
    .id = "wormhole-audio",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_AUDIO,
    .get_name = wh_audio_in_name,
    .create = wh_audio_in_create,
    .destroy = wh_audio_in_destroy,
    .get_defaults = wh_audio_in_defaults,
    .get_properties = wh_audio_in_properties,
    .update = wh_audio_in_update,
};

void wh_register_audio_sources(void)
{
    obs_register_source(&wh_audio_out_info);
    obs_register_source(&wh_audio_in_info);
}

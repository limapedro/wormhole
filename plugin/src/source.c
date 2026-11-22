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
#include <obs-frontend-api.h>
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
    /* One output clone per sender audio source, indexed by track_id. */
    obs_source_t *children[17];
    char child_names[17][128];
    int child_requested[17];
    obs_source_t *local_sources[16];
    struct obs_transform_info local_transforms[16];
    int local_count;
    int local_snapshotted;
    char last_manifest[16 * 1024];
} wh_source_t;

static const char *wh_source_name(void *type_data)
{
    (void)type_data;
    return "WORMHOLE // GAME PC MIRROR";
}

static void wh_source_defaults(obs_data_t *settings)
{
    obs_data_set_default_string(settings, "host", "127.0.0.1");
    obs_data_set_default_int(settings, "port", 18090);
}

/* Forces a fresh connection, which makes the sender resend its scene layout. */
static bool wh_sync_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
    wh_source_t *ctx = data;
    (void)props; (void)p;
    if (ctx) {
        blog(LOG_INFO, "[wormhole] resync requested");
        wh_net_close(ctx->sock);
        ctx->sock = WH_INVALID_SOCKET;
    }
    return false;
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
    obs_properties_t *props = obs_properties_create();
    obs_property_t *p;
    wh_peer_t found[WH_MAX_PEERS];
    size_t count = wh_discovery_peers(found, WH_MAX_PEERS);
    size_t i;

    p = obs_properties_add_text(props, "banner",
        "  ______________________________________________________\n"
        " | W O R M H O L E // G A M E   P C   M I R R O R       |\n"
        " | v0.2.0  ::  mirror scenes + local mic/webcam           |\n"
        " |____________________________________________________|\n",
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_WARNING);

    p = obs_properties_add_text(props, "sec1",
        " ──[ 01 // GAME PC / DISCOVERED NODES ]──────────────────────────", OBS_TEXT_INFO);
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
    obs_properties_add_button2(props, "sync", "> SYNC SCENES FROM SENDER",
                               wh_sync_clicked, data);

    p = obs_properties_add_text(props, "sec2",
        " ──[ 02 // MANUAL LINK ]───────────────────────────────", OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_NORMAL);
    obs_properties_add_text(props, "host", "> HOST / IP", OBS_TEXT_DEFAULT);
    obs_properties_add_int(props, "port", "> TCP PORT", 1024, 65535, 1);

    p = obs_properties_add_text(props, "footer",
        " [ MIRROR MODE: local mic/webcam are preserved ]   [ CYBERPUNK BUILD ]",
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

/* ---------- automatic audio clones on the receiver ---------- */

typedef struct {
    wh_source_t *ctx;
    uint32_t track_id;
    char name[128];
} wh_clone_task_t;

/* Runs on OBS's UI thread: create one audio source per sender source and add
 * it to the active scene so the user can route it like any local source. */
static void wh_create_clone_task(void *param)
{
    wh_clone_task_t *t = param;
    wh_source_t *ctx = t->ctx;
    char unique[192];
    obs_source_t *child = NULL;
    obs_source_t *scene_src;
    obs_scene_t *scene;

    if (t->track_id < 17 && !ctx->children[t->track_id]) {
        snprintf(unique, sizeof(unique), "WH %s", t->name);
        child = obs_get_source_by_name(unique);
        if (!child)
            child = obs_source_create("wormhole-audio", unique, NULL, NULL);
        if (child) {
            scene_src = obs_frontend_get_current_scene();
            if (scene_src) {
                scene = obs_scene_from_source(scene_src);
                if (scene && !obs_scene_find_source(scene, unique))
                    obs_scene_add(scene, child);
                obs_source_release(scene_src);
            }
            ctx->children[t->track_id] = child; /* our own reference */
            snprintf(ctx->child_names[t->track_id],
                     sizeof(ctx->child_names[t->track_id]), "%s", unique);
            blog(LOG_INFO, "[wormhole] created audio clone %u: '%s'",
                 (unsigned)t->track_id, unique);
        }
    }
    bfree(t);
}

static void wh_request_clone(wh_source_t *ctx, uint32_t track_id, const char *name)
{
    wh_clone_task_t *t;

    if (!track_id || track_id >= 17 || ctx->child_requested[track_id])
        return;
    ctx->child_requested[track_id] = 1;
    t = bzalloc(sizeof(*t));
    if (!t)
        return;
    t->ctx = ctx;
    t->track_id = track_id;
    snprintf(t->name, sizeof(t->name), "%s", name ? name : "Audio");
    obs_queue_task(OBS_TASK_UI, wh_create_clone_task, t, false);
}

static void wh_cleanup_clones(void *param)
{
    wh_source_t *ctx = param;
    obs_source_t *scene_src;
    obs_scene_t *scene;
    size_t i;

    scene_src = obs_frontend_get_current_scene();
    scene = scene_src ? obs_scene_from_source(scene_src) : NULL;
    for (i = 1; i < 17; ++i) {
        if (!ctx->children[i])
            continue;
        if (scene) {
            obs_sceneitem_t *item = obs_scene_find_source(scene,
                        obs_source_get_name(ctx->children[i]));
            if (item)
                obs_sceneitem_remove(item);
        }
        obs_source_release(ctx->children[i]);
        ctx->children[i] = NULL;
    }
    if (scene_src)
        obs_source_release(scene_src);
}

static void wh_cleanup_clones_task(void *param)
{
    wh_cleanup_clones(param);
}

/* Snapshot local (non-Wormhole) sources from the current scene BEFORE we
 * create any mirrored scenes. This is what preserves the streaming PC's own
 * microphone and webcam when gameplay comes from the gaming PC. */
static bool wh_snapshot_local(obs_scene_t *scene, obs_sceneitem_t *item, void *param)
{
    wh_source_t *ctx = param;
    obs_source_t *src = obs_sceneitem_get_source(item);
    const char *id;
    (void)scene;
    if (!src || ctx->local_count >= 16)
        return true;
    id = obs_source_get_id(src);
    if (id && strncmp(id, "wormhole", 8) == 0)
        return true;
    ctx->local_sources[ctx->local_count] = obs_source_get_ref(src);
    if (ctx->local_sources[ctx->local_count])
        obs_sceneitem_get_info2(item, &ctx->local_transforms[ctx->local_count]);
    ++ctx->local_count;
    return true;
}

static void wh_snapshot_local_sources(wh_source_t *ctx)
{
    obs_source_t *scene_src;
    obs_scene_t *scene;
    if (ctx->local_snapshotted)
        return;
    ctx->local_snapshotted = 1;
    scene_src = obs_frontend_get_current_scene();
    if (!scene_src)
        return;
    scene = obs_scene_from_source(scene_src);
    if (scene)
        obs_scene_enum_items(scene, wh_snapshot_local, ctx);
    obs_source_release(scene_src);
    blog(LOG_INFO, "[wormhole] preserved %d local source(s) on receiver",
         ctx->local_count);
}

/* Add the snapshotted local sources to a freshly mirrored scene. */
static void wh_add_local_sources(wh_source_t *ctx, obs_scene_t *scene)
{
    int i;
    for (i = 0; i < ctx->local_count; ++i) {
        obs_sceneitem_t *item;
        if (!ctx->local_sources[i])
            continue;
        if (obs_scene_find_source(scene, obs_source_get_name(ctx->local_sources[i])))
            continue;
        item = obs_scene_add(scene, ctx->local_sources[i]);
        if (item)
            obs_sceneitem_set_info2(item, &ctx->local_transforms[i]);
    }
}

static obs_scene_t *wh_mirrored_scene(const char *name)
{
    char unique[384];
    obs_source_t *scene_src;
    obs_scene_t *scene;
    snprintf(unique, sizeof(unique), "WH %s", name);
    scene_src = obs_get_source_by_name(unique);
    if (!scene_src) {
        scene = obs_scene_create(unique);
        scene_src = obs_scene_get_source(scene);
        obs_scene_release(scene);
    }
    if (!scene_src)
        return NULL;
    scene = obs_scene_from_source(scene_src);
    obs_source_release(scene_src);
    return scene;
}

typedef struct {
    wh_source_t *ctx;
    char *text;
} wh_manifest_task_t;

/* Apply a complete state snapshot on OBS's UI thread. Idempotent and
 * non-destructive: existing receiver scenes/sources are never removed. */
static void wh_apply_manifest_task(void *param)
{
    wh_manifest_task_t *task = param;
    char *text = task->text;
    char *cursor = text;
    obs_scene_t *current = NULL;
    obs_source_t *first = NULL;
    char active[256] = "";
    int track_id = 0;
    int audio_idx = 0;
    int bind_audio = 1;
    int video_named = 0;
    int v2 = 1;
    char line[1024];

    wh_snapshot_local_sources(task->ctx);
    while (cursor && *cursor) {
        char *end = strchr(cursor, '\n');
        size_t len = end ? (size_t)(end - cursor) : strlen(cursor);
        if (len >= sizeof(line))
            len = sizeof(line) - 1;
        memcpy(line, cursor, len);
        line[len] = '\0';
        cursor = end ? end + 1 : NULL;

        if (line[0] == 'V' && line[1] == ':') {
            v2 = atoi(line + 2);
            continue;
        }
        if (line[0] == 'A' && line[1] == ':') {
            snprintf(active, sizeof(active), "%s", line + 2);
            continue;
        }
        if (line[0] == 'T' && line[1] == ':') {
            char *bar = strchr(line + 2, '|');
            if (bar) {
                char name[256];
                const char *nstart = bar + 1;
                size_t n = strlen(nstart);
                if (n >= sizeof(name)) n = sizeof(name) - 1;
                memcpy(name, nstart, n);
                name[n] = '\0';
                track_id = atoi(line + 2);
                /* Informational: channels are bound to mirrored clones below. */
            }
            continue;
        }
        if (line[0] == 'S' && line[1] == ':') {
            audio_idx = 0;
            current = wh_mirrored_scene(line + 2);
            if (current) {
                wh_add_local_sources(task->ctx, current);
                if (!obs_scene_find_source(current,
                        obs_source_get_name(task->ctx->source)))
                    obs_scene_add(current, task->ctx->source);
            }
            if (!first) {
                first = obs_scene_get_source(current);
            } else {
                bind_audio = 0; /* channels already bound from first scene */
            }
            continue;
        }
        if (line[0] == 'I' && line[1] == ':') {
            char name[256];
            char kind[32] = "video";
            int visible = 1;
            int muted = 0;
            char *p;
            char *bar;
            if (!current)
                continue;
            snprintf(name, sizeof(name), "%s", line + 2);
            p = strchr(name, '|');
            if (p) {
                *p++ = '\0';
                bar = strchr(p, '|');
                if (bar) *bar++ = '\0';
                snprintf(kind, sizeof(kind), "%s", p);
                if (bar) {
                    visible = atoi(bar);
                    bar = strchr(bar, '|');
                    if (bar) {
                        bar++;
                        bar = strchr(bar, '|');
                        if (bar) muted = atoi(bar + 1);
                    }
                }
            }
            if (strcmp(kind, "audio") == 0) {
                int i;
                obs_source_t *clone = NULL;
                for (i = 1; i < 17; ++i) {
                    if (task->ctx->children[i] &&
                        strcmp(obs_source_get_name(task->ctx->children[i]), name) == 0) {
                        clone = task->ctx->children[i];
                        break;
                    }
                }
                if (!clone) {
                    clone = obs_source_create("wormhole-audio", name, NULL, NULL);
                    if (clone && bind_audio && audio_idx + 1 < 17) {
                        task->ctx->children[audio_idx + 1] = clone;
                        snprintf(task->ctx->child_names[audio_idx + 1], 128, "%s", name);
                        task->ctx->child_requested[audio_idx + 1] = 1;
                        blog(LOG_INFO, "[wormhole] audio channel %d -> '%s'",
                             audio_idx + 1, name);
                    }
                }
                ++audio_idx;
                if (clone) {
                    obs_sceneitem_t *item = obs_scene_find_source(current, name);
                    if (!item)
                        item = obs_scene_add(current, clone);
                    if (item)
                        obs_sceneitem_set_visible(item, visible != 0);
                    obs_source_set_muted(clone, muted != 0);
                }
            } else {
                obs_sceneitem_t *item = obs_scene_find_source(current,
                        obs_source_get_name(task->ctx->source));
                if (!video_named && name[0]) {
                    obs_source_t *taken = obs_get_source_by_name(name);
                    if (!taken) {
                        obs_source_set_name(task->ctx->source, name);
                        blog(LOG_INFO, "[wormhole] video mirror named '%s'", name);
                    }
                    if (taken)
                        obs_source_release(taken);
                    video_named = 1;
                }
                if (item)
                    obs_sceneitem_set_visible(item, visible != 0);
            }
        }
    }

    if (active[0] && v2 >= 1) {
        char unique[384];
        obs_source_t *scene_src;
        snprintf(unique, sizeof(unique), "WH %s", active);
        scene_src = obs_get_source_by_name(unique);
        if (scene_src) {
            obs_frontend_set_current_scene(scene_src);
            obs_source_release(scene_src);
        }
    }
    if (first) {
        obs_source_release(first);
        first = NULL;
    }
    blog(LOG_INFO, "[wormhole] scene manifest applied (v%d, non-destructive)", v2);
    bfree(text);
    bfree(task);
}

static void wh_apply_manifest_wrapper(void *param)
{
    wh_apply_manifest_task(param);
}

static void wh_apply_manifest(wh_source_t *ctx, const char *text)
{
    wh_manifest_task_t *task = bzalloc(sizeof(*task));
    if (!task)
        return;
    task->ctx = ctx;
    task->text = bzalloc(strlen(text) + 1u);
    if (!task->text) {
        bfree(task);
        return;
    }
    strcpy(task->text, text);
    obs_queue_task(OBS_TASK_UI, wh_apply_manifest_wrapper, task, false);
}

/* Minimal source type that carries one sender source's audio on the receiver. */
static const char *wh_audio_clone_name(void *data)
{
    (void)data;
    return "WORMHOLE // AUDIO";
}

static struct obs_source_info wh_audio_clone_info = {
    .id = "wormhole-audio",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_AUDIO,
    .get_name = wh_audio_clone_name,
};

void wh_register_audio_source(void)
{
    obs_register_source(&wh_audio_clone_info);
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
            if (header.type == WH2_MSG_SCENE_MANIFEST) {
                char manifest[64 * 1024];
                if (wh2_parse_text(ctx->frame, len, manifest, sizeof(manifest))) {
                    blog(LOG_INFO, "[wormhole] received scene manifest (%zu bytes)",
                         strlen(manifest));
                    wh_apply_manifest(ctx, manifest);
                }
                continue;
            }
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
                    wh_request_clone(ctx, desc.track_id, desc.name);
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
                if (audio.track_id < 17 && ctx->children[audio.track_id]) {
                    obs_source_output_audio(ctx->children[audio.track_id],
                                            &ctx->audio_frame);
                }
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
    if (obs_in_task_thread(OBS_TASK_UI))
        wh_cleanup_clones(ctx);
    else
        obs_queue_task(OBS_TASK_UI, wh_cleanup_clones_task, ctx, true);
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

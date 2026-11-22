/*
 * Wormhole - low-latency OBS network video/audio plugin
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Initial plugin skeleton. Registers the Wormhole source and output with
 * libobs. The protocol core lives in core/c/ and is shared with the harness.
 */
#include <obs-module.h>

#define WH_VERSION "0.1.0"

typedef struct {
    obs_source_t *source;
    uint64_t frames_received;
    uint64_t audio_packets;
} wh_source_t;

typedef struct {
    obs_output_t *output;
    uint64_t frames_sent;
    uint64_t audio_packets;
} wh_output_t;

static const char *wh_source_name(void *type_data)
{
    (void)type_data;
    return "Wormhole";
}

static void *wh_source_create(obs_data_t *settings, obs_source_t *source)
{
    (void)settings;
    wh_source_t *ctx = bzalloc(sizeof(*ctx));
    ctx->source = source;
    return ctx;
}

static void wh_source_destroy(void *data)
{
    bfree(data);
}

static const char *wh_output_name(void *type_data)
{
    (void)type_data;
    return "Wormhole";
}

static void *wh_output_create(obs_data_t *settings, obs_output_t *output)
{
    (void)settings;
    wh_output_t *ctx = bzalloc(sizeof(*ctx));
    ctx->output = output;
    return ctx;
}

static void wh_output_destroy(void *data)
{
    bfree(data);
}

static bool wh_output_start(void *data)
{
    wh_output_t *ctx = data;
    ctx->frames_sent = 0;
    ctx->audio_packets = 0;
    blog(LOG_INFO, "[wormhole] output started");
    return true;
}

static void wh_output_stop(void *data, uint64_t ts)
{
    (void)ts;
    wh_output_t *ctx = data;
    blog(LOG_INFO, "[wormhole] output stopped: %llu video frames, %llu audio packets",
         (unsigned long long)ctx->frames_sent, (unsigned long long)ctx->audio_packets);
}

static void wh_output_raw_video(void *data, struct video_data *frame)
{
    wh_output_t *ctx = data;
    (void)frame;
    ++ctx->frames_sent;
}

static void wh_output_raw_audio(void *data, struct audio_data *frames)
{
    wh_output_t *ctx = data;
    (void)frames;
    ++ctx->audio_packets;
}

static struct obs_source_info wh_source_info = {
    .id = "wormhole-source",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = OBS_SOURCE_ASYNC_VIDEO | OBS_SOURCE_AUDIO | OBS_SOURCE_DO_NOT_DUPLICATE,
    .get_name = wh_source_name,
    .create = wh_source_create,
    .destroy = wh_source_destroy,
};

static struct obs_output_info wh_output_info = {
    .id = "wormhole-output",
    .flags = OBS_OUTPUT_AV,
    .get_name = wh_output_name,
    .create = wh_output_create,
    .destroy = wh_output_destroy,
    .start = wh_output_start,
    .stop = wh_output_stop,
    .raw_video = wh_output_raw_video,
    .raw_audio = wh_output_raw_audio,
};

OBS_DECLARE_MODULE()

bool obs_module_load(void)
{
    blog(LOG_INFO, "[wormhole] version %s", WH_VERSION);
    obs_register_source(&wh_source_info);
    obs_register_output(&wh_output_info);
    return true;
}

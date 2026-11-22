/*
 * Wormhole - all-C harness: typed configuration and capture tools.
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef WORMHOLE_HARNESS_H
#define WORMHOLE_HARNESS_H

#include <stddef.h>
#include <stdint.h>

#define WH_MAX_PATH 512
#define WH_MAX_VALUE 256

typedef struct {
    char experiment[WH_MAX_VALUE];
    char output_dir[WH_MAX_PATH];
    char dissect_input[WH_MAX_PATH];
    char dissect_output[WH_MAX_PATH];
    size_t max_payload_bytes;

    char proxy_listen[WH_MAX_VALUE];
    char proxy_target[WH_MAX_VALUE];
    char proxy_capture[WH_MAX_PATH];
    char proxy_capture_down[WH_MAX_PATH];
    unsigned delay_ms;
    unsigned drop_percent;
    uint64_t bandwidth_bytes_per_sec;
    size_t chunk_bytes;

    char media_output_dir[WH_MAX_PATH];
    unsigned media_width;
    unsigned media_height;
    unsigned media_frames;
    unsigned media_fps;

    char quality_output[WH_MAX_PATH];
    int expected_r;
    int expected_g;
    int expected_b;

    char differential_left[WH_MAX_PATH];
    char differential_right[WH_MAX_PATH];
    char differential_output[WH_MAX_PATH];
} wh_config_t;

/* Fill with explicit defaults. */
void wh_config_init(wh_config_t *config);

/* Load a TOML subset: [sections], key = string/int/bool. Returns 0 on success. */
int wh_config_load(wh_config_t *config, const char *path);

/* Commands; return 0 on success. */
int wh_cmd_check(const wh_config_t *config);
int wh_cmd_dissect(const wh_config_t *config);
int wh_cmd_synth(const wh_config_t *config);
int wh_cmd_proxy(const wh_config_t *config);
int wh_cmd_quality(const wh_config_t *config);
int wh_cmd_differential(const wh_config_t *config);

#endif /* WORMHOLE_HARNESS_H */

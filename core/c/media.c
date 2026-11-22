/*
 * Wormhole - all-C deterministic synthetic media generator.
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "wormhole/harness.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <direct.h>
#define wh_mkdir(path) _mkdir(path)
#else
#include <sys/stat.h>
#define wh_mkdir(path) mkdir(path, 0777)
#endif

static const unsigned char BAR_Y[8] = {235, 210, 170, 128, 106, 70, 50, 25};
static const unsigned char BAR_U[8] = {128, 90, 40, 128, 170, 220, 240, 128};
static const unsigned char BAR_V[8] = {128, 240, 220, 128, 40, 90, 50, 128};

typedef struct {
    unsigned width;
    unsigned height;
    unsigned fps;
    const char *name;
} profile_t;

static const profile_t PROFILES[5] = {
    {1280, 720, 30, "720p30"},
    {1920, 1080, 30, "1080p30"},
    {1920, 1080, 60, "1080p60"},
    {2560, 1440, 60, "1440p60"},
    {3840, 2160, 30, "4K30"}
};

static int write_i420(const char *dir, const char *name, unsigned width,
                      unsigned height, unsigned frames)
{
    char path[WH_MAX_PATH];
    FILE *file;
    unsigned char *y;
    unsigned char *u;
    unsigned char *v;
    unsigned frame;
    unsigned row;
    unsigned col;
    size_t ysize;
    size_t csize;

    width -= width % 2u;
    height -= height % 2u;
    snprintf(path, sizeof(path), "%s/%s_%ux%u_%uframes.i420", dir, name, width, height, frames);
    file = fopen(path, "wb");
    if (file == NULL)
        return 1;
    ysize = (size_t)width * height;
    csize = (size_t)(width / 2u) * (height / 2u);
    y = (unsigned char *)malloc(ysize);
    u = (unsigned char *)malloc(csize);
    v = (unsigned char *)malloc(csize);
    if (y == NULL || u == NULL || v == NULL) {
        free(y); free(u); free(v); fclose(file);
        return 1;
    }

    for (frame = 0; frame < frames; ++frame) {
        unsigned marker = (frame * 8u) % width;
        for (row = 0; row < height; ++row) {
            for (col = 0; col < width; ++col) {
                unsigned bar = (col * 8u) / width;
                unsigned char luma = BAR_Y[bar];
                if (col >= marker && col < marker + 4u)
                    luma = 255;
                y[(size_t)row * width + col] = luma;
            }
        }
        for (row = 0; row < height / 2u; ++row) {
            for (col = 0; col < width / 2u; ++col) {
                unsigned bar = (col * 16u) / width;
                u[(size_t)row * (width / 2u) + col] = BAR_U[bar];
                v[(size_t)row * (width / 2u) + col] = BAR_V[bar];
            }
        }
        fwrite(y, 1, ysize, file);
        fwrite(u, 1, csize, file);
        fwrite(v, 1, csize, file);
    }
    free(y); free(u); free(v);
    fclose(file);
    return 0;
}

static void write_u32(FILE *file, uint32_t value)
{
    unsigned char b[4] = {
        (unsigned char)(value & 0xffu),
        (unsigned char)((value >> 8) & 0xffu),
        (unsigned char)((value >> 16) & 0xffu),
        (unsigned char)((value >> 24) & 0xffu)
    };
    fwrite(b, 1, 4, file);
}

static void write_u16(FILE *file, uint16_t value)
{
    unsigned char b[2] = {
        (unsigned char)(value & 0xffu),
        (unsigned char)((value >> 8) & 0xffu)
    };
    fwrite(b, 1, 2, file);
}

static int write_wav(const char *dir, unsigned base_hz)
{
    char path[WH_MAX_PATH];
    FILE *file;
    unsigned rate = 48000u;
    unsigned seconds = 2u;
    unsigned total = rate * seconds;
    uint32_t data_bytes = total * 2u * 2u;
    unsigned i;

    snprintf(path, sizeof(path), "%s/track_%u_%uhz.wav", dir, base_hz, base_hz);
    file = fopen(path, "wb");
    if (file == NULL)
        return 1;
    fwrite("RIFF", 1, 4, file);
    write_u32(file, 36u + data_bytes);
    fwrite("WAVEfmt ", 1, 8, file);
    write_u32(file, 16u);
    write_u16(file, 1u);
    write_u16(file, 2u);
    write_u32(file, rate);
    write_u32(file, rate * 2u * 2u);
    write_u16(file, 4u);
    write_u16(file, 16u);
    fwrite("data", 1, 4, file);
    write_u32(file, data_bytes);
    for (i = 0; i < total; ++i) {
        unsigned hz = (i / rate) % 2u == 0 ? base_hz : base_hz * 2u;
        double phase = (double)i * (double)hz * 6.283185307179586 / (double)rate;
        short sample = (short)(sin(phase) * 0.2 * 32767.0);
        write_u16(file, (uint16_t)sample);
        write_u16(file, (uint16_t)sample);
    }
    fclose(file);
    return 0;
}

int wh_cmd_media(const wh_config_t *config)
{
    unsigned i;
    wh_mkdir(config->media_output_dir);
    if (write_i420(config->media_output_dir, "video_main", config->media_width,
                   config->media_height, config->media_frames) != 0)
        return 1;
    for (i = 0; i < 5u; ++i) {
        if (write_i420(config->media_output_dir, PROFILES[i].name, PROFILES[i].width,
                       PROFILES[i].height, 3u) != 0)
            return 1;
    }
    for (i = 0; i < config->media_tracks; ++i) {
        if (write_wav(config->media_output_dir, 440u * (i + 1u)) != 0)
            return 1;
    }
    printf("media: wrote main video, %u profiles and %u tracks to %s\n",
           5u, config->media_tracks, config->media_output_dir);
    return 0;
}

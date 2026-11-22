/*
 * Wormhole - all-C harness: typed TOML config, check, synth and dissect.
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "wormhole/harness.h"
#include "wormhole/protocol.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wh_config_init(wh_config_t *config)
{
    if (config == NULL)
        return;
    memset(config, 0, sizeof(*config));
    snprintf(config->experiment, sizeof(config->experiment), "%s", "exp_000");
    snprintf(config->output_dir, sizeof(config->output_dir), "%s", "exp_results");
    snprintf(config->dissect_input, sizeof(config->dissect_input), "%s", "exp_results/capture.bin");
    snprintf(config->dissect_output, sizeof(config->dissect_output), "%s", "exp_results/dissection.json");
    config->max_payload_bytes = WH_DEFAULT_MAX_PAYLOAD;
    snprintf(config->proxy_listen, sizeof(config->proxy_listen), "%s", "127.0.0.1:18080");
    snprintf(config->proxy_target, sizeof(config->proxy_target), "%s", "127.0.0.1:18081");
    snprintf(config->proxy_capture, sizeof(config->proxy_capture), "%s", "exp_results/capture.bin");
    snprintf(config->proxy_capture_down, sizeof(config->proxy_capture_down), "%s", "exp_results/capture-down.bin");
    config->delay_ms = 0u;
    config->drop_percent = 0u;
    config->bandwidth_bytes_per_sec = 0u;
    config->chunk_bytes = 65536u;
    snprintf(config->media_output_dir, sizeof(config->media_output_dir), "%s", "exp_results/media");
    config->media_width = 1280u;
    config->media_height = 720u;
    config->media_frames = 60u;
    config->media_fps = 30u;
}

static char *trim(char *text)
{
    char *end;
    while (*text != '\0' && isspace((unsigned char)*text))
        ++text;
    end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1]))
        --end;
    *end = '\0';
    return text;
}

static void strip_comment(char *text)
{
    char *hash = strchr(text, '#');
    if (hash != NULL)
        *hash = '\0';
}

static void unquote(char *text)
{
    size_t len = strlen(text);
    if (len >= 2u && text[0] == '"' && text[len - 1u] == '"') {
        text[len - 1u] = '\0';
        memmove(text, text + 1, len - 1u);
    }
}

static void set_string(char *dst, size_t cap, const char *value)
{
    snprintf(dst, cap, "%s", value);
}

static void apply_value(wh_config_t *config, const char *section,
                        const char *key, const char *raw)
{
    char value[WH_MAX_VALUE];
    snprintf(value, sizeof(value), "%s", raw);
    value[sizeof(value) - 1u] = '\0';
    strip_comment(value);
    unquote(value);
    value[strcspn(value, "\r\n")] = '\0';

    if (strcmp(section, "general") == 0) {
        if (strcmp(key, "experiment") == 0)
            set_string(config->experiment, sizeof(config->experiment), value);
        else if (strcmp(key, "output_dir") == 0)
            set_string(config->output_dir, sizeof(config->output_dir), value);
    } else if (strcmp(section, "dissect") == 0) {
        if (strcmp(key, "input") == 0)
            set_string(config->dissect_input, sizeof(config->dissect_input), value);
        else if (strcmp(key, "output") == 0)
            set_string(config->dissect_output, sizeof(config->dissect_output), value);
        else if (strcmp(key, "max_payload_bytes") == 0)
            config->max_payload_bytes = (size_t)strtoull(value, NULL, 10);
    } else if (strcmp(section, "proxy") == 0) {
        if (strcmp(key, "listen") == 0)
            set_string(config->proxy_listen, sizeof(config->proxy_listen), value);
        else if (strcmp(key, "target") == 0)
            set_string(config->proxy_target, sizeof(config->proxy_target), value);
        else if (strcmp(key, "capture") == 0)
            set_string(config->proxy_capture, sizeof(config->proxy_capture), value);
        else if (strcmp(key, "capture_down") == 0)
            set_string(config->proxy_capture_down, sizeof(config->proxy_capture_down), value);
        else if (strcmp(key, "delay_ms") == 0)
            config->delay_ms = (unsigned)strtoul(value, NULL, 10);
        else if (strcmp(key, "drop_percent") == 0)
            config->drop_percent = (unsigned)strtoul(value, NULL, 10);
        else if (strcmp(key, "bandwidth_bytes_per_sec") == 0)
            config->bandwidth_bytes_per_sec = strtoull(value, NULL, 10);
        else if (strcmp(key, "chunk_bytes") == 0)
            config->chunk_bytes = (size_t)strtoull(value, NULL, 10);
    } else if (strcmp(section, "media") == 0) {
        if (strcmp(key, "output_dir") == 0)
            set_string(config->media_output_dir, sizeof(config->media_output_dir), value);
        else if (strcmp(key, "width") == 0)
            config->media_width = (unsigned)strtoul(value, NULL, 10);
        else if (strcmp(key, "height") == 0)
            config->media_height = (unsigned)strtoul(value, NULL, 10);
        else if (strcmp(key, "frames") == 0)
            config->media_frames = (unsigned)strtoul(value, NULL, 10);
        else if (strcmp(key, "fps") == 0)
            config->media_fps = (unsigned)strtoul(value, NULL, 10);
    }
}

int wh_config_load(wh_config_t *config, const char *path)
{
    FILE *file;
    char line[2048];
    char section[128] = "";

    if (config == NULL || path == NULL)
        return 1;
    file = fopen(path, "r");
    if (file == NULL) {
        fprintf(stderr, "error: cannot open config %s\n", path);
        return 1;
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char *text = trim(line);
        char *equals;
        char key[128];
        if (*text == '\0' || *text == '#')
            continue;
        if (*text == '[') {
            char *close = strchr(text, ']');
            if (close == NULL)
                continue;
            *close = '\0';
            snprintf(section, sizeof(section), "%s", text + 1);
            continue;
        }
        equals = strchr(text, '=');
        if (equals == NULL)
            continue;
        *equals = '\0';
        snprintf(key, sizeof(key), "%s", trim(text));
        strip_comment(key);
        apply_value(config, section, key, trim(equals + 1));
    }
    fclose(file);
    return 0;
}

int wh_cmd_check(const wh_config_t *config)
{
    printf("[general]\nexperiment = %s\noutput_dir = %s\n",
           config->experiment, config->output_dir);
    printf("[dissect]\ninput = %s\noutput = %s\nmax_payload_bytes = %zu\n",
           config->dissect_input, config->dissect_output, config->max_payload_bytes);
    printf("[proxy]\nlisten = %s\ntarget = %s\ncapture = %s\ncapture_down = %s\n",
           config->proxy_listen, config->proxy_target, config->proxy_capture,
           config->proxy_capture_down);
    printf("[media]\noutput_dir = %s\nframes = %u\n",
           config->media_output_dir, config->media_frames);
    return 0;
}

static void json_escape(const char *text, char *out, size_t cap)
{
    size_t i = 0;
    size_t j = 0;
    while (text[i] != '\0' && j + 2u < cap) {
        if (text[i] == '\\' || text[i] == '"') {
            out[j++] = '\\';
            out[j++] = text[i];
        } else {
            out[j++] = text[i];
        }
        ++i;
    }
    out[j] = '\0';
}

int wh_cmd_dissect(const wh_config_t *config)
{
    FILE *file = fopen(config->dissect_input, "rb");
    uint8_t *bytes;
    long length;
    size_t offset = 0;
    size_t packets = 0;
    size_t jpeg = 0;
    size_t wave = 0;
    size_t unknown = 0;
    uint8_t *round_trip;
    size_t round_trip_len = 0;
    FILE *report;
    char escaped[WH_MAX_PATH * 2];

    if (file == NULL) {
        fprintf(stderr, "error: cannot read capture %s\n", config->dissect_input);
        return 1;
    }
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    rewind(file);
    if (length <= 0) {
        fclose(file);
        fprintf(stderr, "error: empty capture\n");
        return 1;
    }
    bytes = (uint8_t *)malloc((size_t)length);
    round_trip = (uint8_t *)malloc((size_t)length + WH_HEADER_LEN + WH_IMAGE_HEADER_LEN + 16u);
    if (bytes == NULL || round_trip == NULL ||
        fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        free(round_trip);
        fclose(file);
        return 1;
    }
    fclose(file);

    while (offset < (size_t)length) {
        wh_packet_t packet;
        wh_status_t status = wh_decode(bytes, (size_t)length, &offset, &packet,
                                       config->max_payload_bytes);
        if (status != WH_OK) {
            fprintf(stderr, "error: decode failed at offset %zu (status %d)\n", offset, status);
            free(bytes);
            free(round_trip);
            return 1;
        }
        if (packet.header.kind == WH_KIND_JPEG)
            ++jpeg;
        else if (packet.header.kind == WH_KIND_WAVE)
            ++wave;
        else
            ++unknown;
        {
            size_t encoded = 0;
            if (wh_encode(&packet, round_trip + round_trip_len,
                          (size_t)length + WH_HEADER_LEN + WH_IMAGE_HEADER_LEN + 16u - round_trip_len,
                          &encoded) != WH_OK) {
                wh_packet_free(&packet);
                free(bytes);
                free(round_trip);
                return 1;
            }
            round_trip_len += encoded;
        }
        wh_packet_free(&packet);
        ++packets;
    }

    if (round_trip_len != (size_t)length ||
        memcmp(bytes, round_trip, (size_t)length) != 0) {
        size_t mismatch = 0;
        size_t limit = round_trip_len < (size_t)length ? round_trip_len : (size_t)length;
        while (mismatch < limit && bytes[mismatch] == round_trip[mismatch])
            ++mismatch;
        fprintf(stderr,
                "error: decode/re-encode was not byte-identical (length %zu vs %zu, first mismatch at byte %zu: %02x vs %02x)\n",
                (size_t)length, round_trip_len, mismatch,
                mismatch < (size_t)length ? bytes[mismatch] : 0,
                mismatch < round_trip_len ? round_trip[mismatch] : 0);
        free(bytes);
        free(round_trip);
        return 1;
    }

    report = fopen(config->dissect_output, "w");
    if (report == NULL) {
        free(bytes);
        free(round_trip);
        return 1;
    }
    json_escape(config->experiment, escaped, sizeof(escaped));
    fprintf(report, "{\n  \"experiment\": \"%s\",\n", escaped);
    json_escape(config->dissect_input, escaped, sizeof(escaped));
    fprintf(report, "  \"input\": \"%s\",\n", escaped);
    fprintf(report, "  \"packet_count\": %zu,\n  \"byte_count\": %zu,\n", packets, (size_t)length);
    fprintf(report, "  \"jpeg\": %zu,\n  \"wave\": %zu,\n  \"unknown\": %zu,\n", jpeg, wave, unknown);
    fprintf(report, "  \"round_trip_ok\": true\n}\n");
    fclose(report);

    printf("dissect: %zu packets (%zu JPEG, %zu WAVE, %zu other), %zu bytes, byte-identical\n",
           packets, jpeg, wave, unknown, (size_t)length);
    printf("wrote %s\n", config->dissect_output);
    free(bytes);
    free(round_trip);
    return 0;
}

int wh_cmd_synth(const wh_config_t *config)
{
    FILE *file = fopen(config->dissect_input, "wb");
    unsigned frame;
    if (file == NULL) {
        fprintf(stderr, "error: cannot write %s\n", config->dissect_input);
        return 1;
    }
    for (frame = 0; frame < config->media_frames; ++frame) {
        uint8_t buffer[512];
        size_t written = 0;
        size_t i;
        wh_packet_t packet;
        wh_packet_init(&packet);
        wh_kind_to_bytes(WH_KIND_JPEG, packet.header.type);
        packet.header.kind = WH_KIND_JPEG;
        packet.header.timestamp = (uint64_t)frame * 1000000000ULL / config->media_fps;
        packet.header.size = 16;
        packet.image.color_range_max[0] = 1.0f;
        packet.image.color_range_max[1] = 1.0f;
        packet.image.color_range_max[2] = 1.0f;
        packet.payload_len = 16u;
        packet.payload = (uint8_t *)malloc(16u);
        if (packet.payload == NULL)
            return 1;
        for (i = 0; i < 16u; ++i)
            packet.payload[i] = (uint8_t)(frame + i);
        if (wh_encode(&packet, buffer, sizeof(buffer), &written) != WH_OK ||
            fwrite(buffer, 1, written, file) != written)
            return 1;
        wh_packet_free(&packet);

        wh_packet_init(&packet);
        wh_kind_to_bytes(WH_KIND_WAVE, packet.header.type);
        packet.header.kind = WH_KIND_WAVE;
        packet.header.timestamp = (uint64_t)frame * 1000000000ULL / config->media_fps;
        packet.header.size = 8;
        packet.wave.format = 4;
        packet.wave.sample_rate = 48000;
        packet.wave.speakers = 2;
        packet.wave.frames = 2;
        packet.payload_len = 8u;
        packet.payload = (uint8_t *)malloc(8u);
        if (packet.payload == NULL)
            return 1;
        for (i = 0; i < 8u; ++i)
            packet.payload[i] = (uint8_t)(frame + i);
        if (wh_encode(&packet, buffer, sizeof(buffer), &written) != WH_OK ||
            fwrite(buffer, 1, written, file) != written)
            return 1;
        wh_packet_free(&packet);
    }
    fclose(file);
    printf("synth: wrote %u frames to %s\n", config->media_frames, config->dissect_input);
    return 0;
}

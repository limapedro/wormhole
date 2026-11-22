/*
 * Wormhole - all-C capture comparison (differential tester stage 1).
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "wormhole/harness.h"
#include "wormhole/protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *read_capture(const char *path, size_t *length)
{
    FILE *file = fopen(path, "rb");
    uint8_t *bytes;
    long size;
    if (file == NULL)
        return NULL;
    fseek(file, 0, SEEK_END);
    size = ftell(file);
    rewind(file);
    if (size < 0) {
        fclose(file);
        return NULL;
    }
    bytes = (uint8_t *)malloc((size_t)size);
    if (bytes == NULL || (size > 0 && fread(bytes, 1, (size_t)size, file) != (size_t)size)) {
        free(bytes);
        fclose(file);
        return NULL;
    }
    fclose(file);
    *length = (size_t)size;
    return bytes;
}

static uint32_t fnv1a(const uint8_t *data, size_t len)
{
    uint32_t hash = 0x811C9DC5u;
    size_t i;
    for (i = 0; i < len; ++i) {
        hash ^= (uint32_t)data[i];
        hash *= 0x01000193u;
    }
    return hash;
}

int wh_cmd_differential(const wh_config_t *config)
{
    size_t left_len = 0;
    size_t right_len = 0;
    uint8_t *left = read_capture(config->differential_left, &left_len);
    uint8_t *right = read_capture(config->differential_right, &right_len);
    size_t loff = 0;
    size_t roff = 0;
    size_t left_count = 0;
    size_t right_count = 0;
    size_t compared = 0;
    size_t differences = 0;
    FILE *report;

    if (left == NULL || right == NULL) {
        fprintf(stderr, "error: cannot read differential captures\n");
        free(left);
        free(right);
        return 1;
    }
    while (loff < left_len) {
        wh_packet_t packet;
        if (wh_decode(left, left_len, &loff, &packet, config->max_payload_bytes) != WH_OK) {
            free(left);
            free(right);
            return 1;
        }
        ++left_count;
        wh_packet_free(&packet);
    }
    while (roff < right_len) {
        wh_packet_t packet;
        if (wh_decode(right, right_len, &roff, &packet, config->max_payload_bytes) != WH_OK) {
            free(left);
            free(right);
            return 1;
        }
        ++right_count;
        wh_packet_free(&packet);
    }

    loff = 0;
    roff = 0;
    while (loff < left_len && roff < right_len) {
        wh_packet_t l;
        wh_packet_t r;
        wh_packet_init(&l);
        wh_packet_init(&r);
        if (wh_decode(left, left_len, &loff, &l, config->max_payload_bytes) != WH_OK ||
            wh_decode(right, right_len, &roff, &r, config->max_payload_bytes) != WH_OK) {
            wh_packet_free(&l);
            wh_packet_free(&r);
            free(left);
            free(right);
            return 1;
        }
        if (l.header.kind != r.header.kind || l.header.timestamp != r.header.timestamp ||
            fnv1a(l.payload, l.payload_len) != fnv1a(r.payload, r.payload_len))
            ++differences;
        ++compared;
        wh_packet_free(&l);
        wh_packet_free(&r);
    }
    if (left_count != right_count)
        ++differences;

    report = fopen(config->differential_output, "w");
    if (report == NULL) {
        free(left);
        free(right);
        return 1;
    }
    fprintf(report, "{\n  \"left\": \"%s\",\n  \"right\": \"%s\",\n",
            config->differential_left, config->differential_right);
    fprintf(report, "  \"left_packets\": %zu,\n  \"right_packets\": %zu,\n",
            left_count, right_count);
    fprintf(report, "  \"compared\": %zu,\n  \"differences\": %zu\n}\n", compared, differences);
    fclose(report);
    printf("differential: %zu vs %zu packets, %zu compared, %zu difference(s)\n",
           left_count, right_count, compared, differences);
    printf("wrote %s\n", config->differential_output);
    free(left);
    free(right);
    return differences == 0 ? 0 : 1;
}

/*
 * Wormhole - low-latency OBS network video/audio plugin
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "wormhole/protocol.h"

#include <stdlib.h>
#include <string.h>

static uint32_t read_u32_le(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t read_u64_le(const uint8_t *p)
{
    return (uint64_t)read_u32_le(p) | ((uint64_t)read_u32_le(p + 4) << 32);
}

static void write_u32_le(uint8_t *p, uint32_t value)
{
    p[0] = (uint8_t)(value & 0xffu);
    p[1] = (uint8_t)((value >> 8) & 0xffu);
    p[2] = (uint8_t)((value >> 16) & 0xffu);
    p[3] = (uint8_t)((value >> 24) & 0xffu);
}

static void write_u64_le(uint8_t *p, uint64_t value)
{
    write_u32_le(p, (uint32_t)(value & 0xffffffffu));
    write_u32_le(p + 4, (uint32_t)(value >> 32));
}

static float read_f32_le(const uint8_t *p)
{
    float value;
    uint32_t bits = read_u32_le(p);
    memcpy(&value, &bits, sizeof(value));
    return value;
}

static void write_f32_le(uint8_t *p, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof(bits));
    write_u32_le(p, bits);
}

wh_kind_t wh_kind_from_bytes(const uint8_t bytes[4])
{
    if (bytes[0] == 'J' && bytes[1] == 'P' && bytes[2] == 'E' && bytes[3] == 'G')
        return WH_KIND_JPEG;
    if (bytes[0] == 'W' && bytes[1] == 'A' && bytes[2] == 'V' && bytes[3] == 'E')
        return WH_KIND_WAVE;
    if (bytes[0] == 'A' && bytes[1] == 'N' && bytes[2] == 'J' && bytes[3] == 'A')
        return WH_KIND_ANJA;
    return WH_KIND_UNKNOWN;
}

void wh_kind_to_bytes(wh_kind_t kind, uint8_t out[4])
{
    static const uint8_t jpeg[4] = {'J', 'P', 'E', 'G'};
    static const uint8_t wave[4] = {'W', 'A', 'V', 'E'};
    static const uint8_t anja[4] = {'A', 'N', 'J', 'A'};

    switch (kind) {
    case WH_KIND_JPEG: memcpy(out, jpeg, 4); break;
    case WH_KIND_WAVE: memcpy(out, wave, 4); break;
    case WH_KIND_ANJA: memcpy(out, anja, 4); break;
    default: memset(out, 0, 4); break;
    }
}

size_t wh_header_extra_len(wh_kind_t kind)
{
    if (kind == WH_KIND_JPEG)
        return WH_IMAGE_HEADER_LEN;
    if (kind == WH_KIND_WAVE)
        return WH_WAVE_HEADER_LEN;
    return 0u;
}

void wh_packet_init(wh_packet_t *packet)
{
    if (packet == NULL)
        return;
    memset(packet, 0, sizeof(*packet));
}

void wh_packet_free(wh_packet_t *packet)
{
    if (packet == NULL)
        return;
    free(packet->payload);
    packet->payload = NULL;
    packet->payload_len = 0u;
}

wh_status_t wh_decode(const uint8_t *data, size_t len, size_t *offset,
                      wh_packet_t *out, size_t max_payload)
{
    size_t pos;
    size_t extra;
    size_t size;
    size_t i;

    if (data == NULL || offset == NULL || out == NULL)
        return WH_ERR_NULL;
    pos = *offset;
    if (pos > len || len - pos < WH_HEADER_LEN)
        return WH_ERR_EOF;

    wh_packet_init(out);
    memcpy(out->header.type, data + pos, 4);
    out->header.kind = wh_kind_from_bytes(out->header.type);
    out->header.timestamp = read_u64_le(data + pos + 4);
    out->header.size = (int32_t)read_u32_le(data + pos + 12);
    if (out->header.size < 0)
        return WH_ERR_INVALID_SIZE;

    size = (size_t)out->header.size;
    if (size > max_payload)
        return WH_ERR_TOO_LARGE;
    extra = wh_header_extra_len(out->header.kind);
    if (len - pos < WH_HEADER_LEN + extra + size)
        return WH_ERR_EOF;

    if (out->header.kind == WH_KIND_JPEG) {
        for (i = 0; i < 16u; ++i)
            out->image.color_matrix[i] = read_f32_le(data + pos + WH_HEADER_LEN + i * 4u);
        for (i = 0; i < 3u; ++i)
            out->image.color_range_min[i] = read_f32_le(data + pos + WH_HEADER_LEN + 64u + i * 4u);
        for (i = 0; i < 3u; ++i)
            out->image.color_range_max[i] = read_f32_le(data + pos + WH_HEADER_LEN + 76u + i * 4u);
    } else if (out->header.kind == WH_KIND_WAVE) {
        out->wave.format = (int32_t)read_u32_le(data + pos + WH_HEADER_LEN);
        out->wave.sample_rate = (int32_t)read_u32_le(data + pos + WH_HEADER_LEN + 4u);
        out->wave.speakers = (int32_t)read_u32_le(data + pos + WH_HEADER_LEN + 8u);
        out->wave.frames = (int32_t)read_u32_le(data + pos + WH_HEADER_LEN + 12u);
    }

    out->payload_len = size;
    if (size > 0u) {
        out->payload = (uint8_t *)malloc(size);
        if (out->payload == NULL)
            return WH_ERR_BUFFER_TOO_SMALL;
        memcpy(out->payload, data + pos + WH_HEADER_LEN + extra, size);
    }

    *offset = pos + WH_HEADER_LEN + extra + size;
    return WH_OK;
}

wh_status_t wh_encode(const wh_packet_t *packet, uint8_t *out, size_t cap,
                      size_t *written)
{
    size_t extra;
    size_t total;
    size_t i;
    size_t pos;

    if (packet == NULL || out == NULL || written == NULL)
        return WH_ERR_NULL;
    if (packet->header.size < 0)
        return WH_ERR_INVALID_SIZE;

    extra = wh_header_extra_len(packet->header.kind);
    total = WH_HEADER_LEN + extra + (size_t)packet->header.size;
    if (cap < total)
        return WH_ERR_BUFFER_TOO_SMALL;

    /* Preserve the original four bytes so unknown types round-trip exactly. */
    memcpy(out, packet->header.type, 4);
    write_u64_le(out + 4, packet->header.timestamp);
    write_u32_le(out + 12, (uint32_t)packet->header.size);
    pos = WH_HEADER_LEN;

    if (packet->header.kind == WH_KIND_JPEG) {
        for (i = 0; i < 16u; ++i)
            write_f32_le(out + pos + i * 4u, packet->image.color_matrix[i]);
        for (i = 0; i < 3u; ++i)
            write_f32_le(out + pos + 64u + i * 4u, packet->image.color_range_min[i]);
        for (i = 0; i < 3u; ++i)
            write_f32_le(out + pos + 76u + i * 4u, packet->image.color_range_max[i]);
        pos += WH_IMAGE_HEADER_LEN;
    } else if (packet->header.kind == WH_KIND_WAVE) {
        write_u32_le(out + pos, (uint32_t)packet->wave.format);
        write_u32_le(out + pos + 4u, (uint32_t)packet->wave.sample_rate);
        write_u32_le(out + pos + 8u, (uint32_t)packet->wave.speakers);
        write_u32_le(out + pos + 12u, (uint32_t)packet->wave.frames);
        pos += WH_WAVE_HEADER_LEN;
    }

    if (packet->header.size > 0) {
        if (packet->payload == NULL)
            return WH_ERR_NULL;
        memcpy(out + pos, packet->payload, (size_t)packet->header.size);
    }
    *written = total;
    return WH_OK;
}

int wh_image_is_full_range(const wh_image_header_t *image)
{
    if (image == NULL)
        return 0;
    return image->color_range_min[0] == 0.0f &&
           image->color_range_min[1] == 0.0f &&
           image->color_range_min[2] == 0.0f &&
           image->color_range_max[0] == 1.0f &&
           image->color_range_max[1] == 1.0f &&
           image->color_range_max[2] == 1.0f;
}

int wh_wave_channels(int32_t speakers)
{
    switch (speakers) {
    case 1: return 1;
    case 2: return 2;
    case 3: return 3;
    case 4: return 4;
    case 5: return 5;
    case 6: return 6;
    case 8: return 8;
    default: return 0;
    }
}

int wh_wave_bytes_per_sample(int32_t format)
{
    switch (format) {
    case 1: case 5: return 1;
    case 2: case 6: return 2;
    case 3: case 4: case 7: case 8: return 4;
    default: return 0;
    }
}

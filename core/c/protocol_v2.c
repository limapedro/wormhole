/* Wormhole protocol v2 implementation. Copyright (C) 2026 Wormhole contributors. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/protocol_v2.h"

#include <string.h>

static void put_u16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)(v >> 8);
}

static void put_u32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xffu);
    p[1] = (uint8_t)((v >> 8) & 0xffu);
    p[2] = (uint8_t)((v >> 16) & 0xffu);
    p[3] = (uint8_t)((v >> 24) & 0xffu);
}

static void put_u64(uint8_t *p, uint64_t v)
{
    put_u32(p, (uint32_t)(v & 0xffffffffu));
    put_u32(p + 4, (uint32_t)(v >> 32));
}

static uint16_t get_u16(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t get_u32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint64_t get_u64(const uint8_t *p)
{
    return (uint64_t)get_u32(p) | ((uint64_t)get_u32(p + 4) << 32);
}

int wh2_header_encode(uint8_t *out, size_t cap, const wh2_header_t *header)
{
    if (!out || !header || cap < WH2_HEADER_LEN)
        return 0;
    memcpy(out, WH2_MAGIC, 4);
    put_u16(out + 4, header->version);
    put_u16(out + 6, header->type);
    put_u32(out + 8, header->total_len);
    put_u32(out + 12, header->sequence);
    put_u64(out + 16, header->timestamp_ns);
    return 1;
}

wh2_parse_status_t wh2_header_decode(const uint8_t *data, size_t len, wh2_header_t *out)
{
    if (!data || !out || len < WH2_HEADER_LEN)
        return WH2_PARSE_TRUNCATED;
    if (memcmp(data, WH2_MAGIC, 4) != 0)
        return WH2_PARSE_BAD_MAGIC;
    out->version = get_u16(data + 4);
    if (out->version != WH2_VERSION)
        return WH2_PARSE_BAD_VERSION;
    out->type = get_u16(data + 6);
    out->total_len = get_u32(data + 8);
    if (out->total_len < WH2_HEADER_LEN || out->total_len > WH2_MAX_FRAME ||
        out->total_len > len)
        return WH2_PARSE_BAD_LENGTH;
    out->sequence = get_u32(data + 12);
    out->timestamp_ns = get_u64(data + 16);
    return WH2_PARSE_OK;
}

static size_t begin_frame(uint8_t *out, size_t cap, uint16_t type,
                          uint32_t sequence, uint64_t timestamp_ns)
{
    wh2_header_t header;
    memset(&header, 0, sizeof(header));
    header.version = WH2_VERSION;
    header.type = type;
    header.sequence = sequence;
    header.timestamp_ns = timestamp_ns;
    if (!wh2_header_encode(out, cap, &header))
        return 0;
    return WH2_HEADER_LEN;
}

static size_t put_string(uint8_t *out, size_t cap, size_t at, const char *text)
{
    size_t len = text ? strlen(text) : 0;
    if (len > 65535u || at + 2u + len > cap)
        return 0;
    put_u16(out + at, (uint16_t)len);
    if (len)
        memcpy(out + at + 2, text, len);
    return at + 2u + len;
}

static size_t get_string(const uint8_t *data, size_t len, size_t at,
                         char *dst, size_t dst_cap)
{
    uint16_t n;
    size_t copy;
    if (at + 2u > len)
        return 0;
    n = get_u16(data + at);
    at += 2u;
    if (at + n > len)
        return 0;
    copy = n;
    if (copy >= dst_cap)
        copy = dst_cap - 1u;
    if (dst && dst_cap) {
        memcpy(dst, data + at, copy);
        dst[copy] = '\0';
    }
    return at + n;
}

size_t wh2_build_hello(uint8_t *out, size_t cap, uint32_t sequence,
                       uint64_t timestamp_ns, uint32_t capabilities,
                       const char *name, uint8_t track_count,
                       uint32_t width, uint32_t height,
                       uint32_t fps_num, uint32_t fps_den)
{
    size_t at = begin_frame(out, cap, WH2_MSG_HELLO, sequence, timestamp_ns);
    if (!at || at + 4u + 1u + 16u > cap)
        return 0;
    put_u32(out + at, capabilities);
    at += 4u;
    out[at++] = track_count;
    at = put_string(out, cap, at, name);
    if (!at || at + 16u > cap)
        return 0;
    put_u32(out + at, width); at += 4;
    put_u32(out + at, height); at += 4;
    put_u32(out + at, fps_num); at += 4;
    put_u32(out + at, fps_den); at += 4;
    put_u32(out + 8, (uint32_t)at);
    return at;
}

size_t wh2_build_hello_ack(uint8_t *out, size_t cap, uint32_t sequence,
                           uint64_t timestamp_ns, uint32_t capabilities,
                           const char *sender_name)
{
    size_t at = begin_frame(out, cap, WH2_MSG_HELLO_ACK, sequence, timestamp_ns);
    if (!at || at + 2u + 4u > cap)
        return 0;
    put_u16(out + at, WH2_VERSION); at += 2;
    put_u32(out + at, capabilities); at += 4;
    at = put_string(out, cap, at, sender_name);
    if (!at)
        return 0;
    put_u32(out + 8, (uint32_t)at);
    return at;
}

size_t wh2_build_stream_desc(uint8_t *out, size_t cap, uint32_t sequence,
                             uint64_t timestamp_ns, uint32_t track_id,
                             const char *name, uint32_t sample_rate,
                             uint16_t channels, uint16_t bits,
                             uint32_t clock_id, uint32_t codec_fourcc)
{
    size_t at = begin_frame(out, cap, WH2_MSG_STREAM_DESC, sequence, timestamp_ns);
    if (!at || at + 4u > cap)
        return 0;
    put_u32(out + at, track_id); at += 4;
    at = put_string(out, cap, at, name);
    if (!at || at + 4u + 2u + 2u + 4u + 4u > cap)
        return 0;
    put_u32(out + at, sample_rate); at += 4;
    put_u16(out + at, channels); at += 2;
    put_u16(out + at, bits); at += 2;
    put_u32(out + at, clock_id); at += 4;
    put_u32(out + at, codec_fourcc); at += 4;
    put_u32(out + 8, (uint32_t)at);
    return at;
}

size_t wh2_build_video(uint8_t *out, size_t cap, uint32_t sequence,
                       uint64_t timestamp_ns, uint64_t frame_seq,
                       uint32_t width, uint32_t height,
                       const uint8_t *jpeg, size_t jpeg_len)
{
    size_t at = begin_frame(out, cap, WH2_MSG_VIDEO, sequence, timestamp_ns);
    if (!at || at + 8u + 8u > cap || jpeg_len > cap - at - 16u)
        return 0;
    put_u64(out + at, frame_seq); at += 8;
    put_u32(out + at, width); at += 4;
    put_u32(out + at, height); at += 4;
    if (jpeg_len)
        memcpy(out + at, jpeg, jpeg_len);
    at += jpeg_len;
    put_u32(out + 8, (uint32_t)at);
    return at;
}

size_t wh2_build_audio(uint8_t *out, size_t cap, uint32_t sequence,
                       uint64_t timestamp_ns, uint32_t track_id,
                       uint32_t frames, uint32_t sample_rate,
                       uint16_t channels, uint16_t bits,
                       const uint8_t *pcm, size_t pcm_len)
{
    size_t at = begin_frame(out, cap, WH2_MSG_AUDIO, sequence, timestamp_ns);
    if (!at || at + 4u + 4u + 4u + 2u + 2u > cap || pcm_len > cap - at - 16u)
        return 0;
    put_u32(out + at, track_id); at += 4;
    put_u32(out + at, frames); at += 4;
    put_u32(out + at, sample_rate); at += 4;
    put_u16(out + at, channels); at += 2;
    put_u16(out + at, bits); at += 2;
    if (pcm_len)
        memcpy(out + at, pcm, pcm_len);
    at += pcm_len;
    put_u32(out + 8, (uint32_t)at);
    return at;
}

static wh2_parse_status_t common(const uint8_t *data, size_t len,
                                 wh2_header_t *header, uint16_t expected)
{
    wh2_parse_status_t status = wh2_header_decode(data, len, header);
    if (status != WH2_PARSE_OK)
        return status;
    if (header->type != expected)
        return WH2_PARSE_BAD_TYPE;
    return WH2_PARSE_OK;
}

wh2_parse_status_t wh2_parse_hello(const uint8_t *data, size_t len, wh2_hello_t *out)
{
    wh2_parse_status_t status;
    size_t at;
    if (!out)
        return WH2_PARSE_TRUNCATED;
    status = common(data, len, &out->header, WH2_MSG_HELLO);
    if (status != WH2_PARSE_OK)
        return status;
    at = WH2_HEADER_LEN;
    if (at + 5u > len)
        return WH2_PARSE_TRUNCATED;
    out->capabilities = get_u32(data + at); at += 4;
    out->track_count = data[at++];
    at = get_string(data, len, at, out->name, sizeof(out->name));
    if (!at || at + 16u > len)
        return WH2_PARSE_TRUNCATED;
    out->width = get_u32(data + at); at += 4;
    out->height = get_u32(data + at); at += 4;
    out->fps_num = get_u32(data + at); at += 4;
    out->fps_den = get_u32(data + at);
    return WH2_PARSE_OK;
}

wh2_parse_status_t wh2_parse_hello_ack(const uint8_t *data, size_t len, wh2_hello_ack_t *out)
{
    wh2_parse_status_t status;
    size_t at = WH2_HEADER_LEN;
    if (!out)
        return WH2_PARSE_TRUNCATED;
    status = common(data, len, &out->header, WH2_MSG_HELLO_ACK);
    if (status != WH2_PARSE_OK)
        return status;
    if (at + 6u > len)
        return WH2_PARSE_TRUNCATED;
    out->accepted_version = get_u16(data + at); at += 2;
    out->capabilities = get_u32(data + at); at += 4;
    out->sender_name[0] = '\0';
    if (at < out->header.total_len)
        get_string(data, out->header.total_len, at, out->sender_name,
                   sizeof(out->sender_name));
    return WH2_PARSE_OK;
}

wh2_parse_status_t wh2_parse_stream_desc(const uint8_t *data, size_t len, wh2_stream_desc_t *out)
{
    wh2_parse_status_t status;
    size_t at = WH2_HEADER_LEN;
    if (!out)
        return WH2_PARSE_TRUNCATED;
    status = common(data, len, &out->header, WH2_MSG_STREAM_DESC);
    if (status != WH2_PARSE_OK)
        return status;
    if (at + 4u > len)
        return WH2_PARSE_TRUNCATED;
    out->track_id = get_u32(data + at); at += 4;
    at = get_string(data, len, at, out->name, sizeof(out->name));
    if (!at || at + 16u > len)
        return WH2_PARSE_TRUNCATED;
    out->sample_rate = get_u32(data + at); at += 4;
    out->channels = get_u16(data + at); at += 2;
    out->bits = get_u16(data + at); at += 2;
    out->clock_id = get_u32(data + at); at += 4;
    out->codec_fourcc = get_u32(data + at);
    return WH2_PARSE_OK;
}

wh2_parse_status_t wh2_parse_video(const uint8_t *data, size_t len, wh2_video_t *out)
{
    wh2_parse_status_t status;
    size_t at = WH2_HEADER_LEN;
    if (!out)
        return WH2_PARSE_TRUNCATED;
    status = common(data, len, &out->header, WH2_MSG_VIDEO);
    if (status != WH2_PARSE_OK)
        return status;
    if (at + 16u > len || len < out->header.total_len ||
        out->header.total_len < at)
        return WH2_PARSE_TRUNCATED;
    out->sequence64 = get_u64(data + at); at += 8;
    out->width = get_u32(data + at); at += 4;
    out->height = get_u32(data + at); at += 4;
    out->payload = data + at;
    out->payload_len = out->header.total_len - at;
    return WH2_PARSE_OK;
}

wh2_parse_status_t wh2_parse_audio(const uint8_t *data, size_t len, wh2_audio_t *out)
{
    wh2_parse_status_t status;
    size_t at = WH2_HEADER_LEN;
    if (!out)
        return WH2_PARSE_TRUNCATED;
    status = common(data, len, &out->header, WH2_MSG_AUDIO);
    if (status != WH2_PARSE_OK)
        return status;
    if (at + 16u > len || len < out->header.total_len ||
        out->header.total_len < at)
        return WH2_PARSE_TRUNCATED;
    out->track_id = get_u32(data + at); at += 4;
    out->frames = get_u32(data + at); at += 4;
    out->sample_rate = get_u32(data + at); at += 4;
    out->channels = get_u16(data + at); at += 2;
    out->bits = get_u16(data + at); at += 2;
    out->payload = data + at;
    out->payload_len = out->header.total_len - at;
    return WH2_PARSE_OK;
}

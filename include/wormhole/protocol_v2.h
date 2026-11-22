/*
 * Wormhole protocol v2 framing. MIT-compatible project license: GPL-2.0-or-later.
 * All lengths are little-endian and bounded; no parser reads past its input.
 */
#ifndef WORMHOLE_PROTOCOL_V2_H
#define WORMHOLE_PROTOCOL_V2_H

#include <stddef.h>
#include <stdint.h>

#define WH2_MAGIC "WH02"
#define WH2_VERSION 2u
#define WH2_HEADER_LEN 28u
#define WH2_MAX_FRAME (64u * 1024u * 1024u)

#define WH2_CAP_MULTI_AUDIO (1u << 0)
#define WH2_CAP_SIMD_JPEG   (1u << 1)
#define WH2_CAP_CLOCK_SYNC  (1u << 2)

typedef enum {
    WH2_MSG_HELLO = 1,
    WH2_MSG_HELLO_ACK = 2,
    WH2_MSG_STREAM_DESC = 3,
    WH2_MSG_VIDEO = 16,
    WH2_MSG_AUDIO = 17,
    WH2_MSG_PING = 18,
    WH2_MSG_GOODBYE = 19
} wh2_message_type_t;

typedef struct {
    uint16_t version;
    uint16_t type;
    uint32_t total_len;
    uint32_t sequence;
    uint64_t timestamp_ns;
} wh2_header_t;

typedef struct {
    wh2_header_t header;
    uint32_t capabilities;
    char name[128];
    uint8_t track_count;
    uint32_t width;
    uint32_t height;
    uint32_t fps_num;
    uint32_t fps_den;
} wh2_hello_t;

typedef struct {
    wh2_header_t header;
    uint16_t accepted_version;
    uint32_t capabilities;
    char sender_name[128];
} wh2_hello_ack_t;

typedef struct {
    wh2_header_t header;
    uint32_t track_id;
    char name[128];
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits;
    uint32_t clock_id;
    uint32_t codec_fourcc;
} wh2_stream_desc_t;

typedef struct {
    wh2_header_t header;
    uint64_t sequence64;
    uint32_t width;
    uint32_t height;
    const uint8_t *payload;
    size_t payload_len;
} wh2_video_t;

typedef struct {
    wh2_header_t header;
    uint32_t track_id;
    uint32_t frames;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bits;
    const uint8_t *payload;
    size_t payload_len;
} wh2_audio_t;

typedef enum {
    WH2_PARSE_OK = 0,
    WH2_PARSE_BAD_MAGIC = 1,
    WH2_PARSE_BAD_VERSION = 2,
    WH2_PARSE_BAD_LENGTH = 3,
    WH2_PARSE_TRUNCATED = 4,
    WH2_PARSE_BAD_TYPE = 5
} wh2_parse_status_t;

/* Header helpers. */
int wh2_header_encode(uint8_t *out, size_t cap, const wh2_header_t *header);
wh2_parse_status_t wh2_header_decode(const uint8_t *data, size_t len, wh2_header_t *out);

/* Builders return the number of bytes written, or 0 on error. */
size_t wh2_build_hello(uint8_t *out, size_t cap, uint32_t sequence,
                       uint64_t timestamp_ns, uint32_t capabilities,
                       const char *name, uint8_t track_count,
                       uint32_t width, uint32_t height,
                       uint32_t fps_num, uint32_t fps_den);
size_t wh2_build_hello_ack(uint8_t *out, size_t cap, uint32_t sequence,
                           uint64_t timestamp_ns, uint32_t capabilities,
                           const char *sender_name);
size_t wh2_build_stream_desc(uint8_t *out, size_t cap, uint32_t sequence,
                             uint64_t timestamp_ns, uint32_t track_id,
                             const char *name, uint32_t sample_rate,
                             uint16_t channels, uint16_t bits,
                             uint32_t clock_id, uint32_t codec_fourcc);
size_t wh2_build_video(uint8_t *out, size_t cap, uint32_t sequence,
                       uint64_t timestamp_ns, uint64_t frame_seq,
                       uint32_t width, uint32_t height,
                       const uint8_t *jpeg, size_t jpeg_len);
size_t wh2_build_audio(uint8_t *out, size_t cap, uint32_t sequence,
                       uint64_t timestamp_ns, uint32_t track_id,
                       uint32_t frames, uint32_t sample_rate,
                       uint16_t channels, uint16_t bits,
                       const uint8_t *pcm, size_t pcm_len);

/* Parsers fill the caller-provided structs; payload pointers alias `data`. */
wh2_parse_status_t wh2_parse_hello(const uint8_t *data, size_t len, wh2_hello_t *out);
wh2_parse_status_t wh2_parse_hello_ack(const uint8_t *data, size_t len, wh2_hello_ack_t *out);
wh2_parse_status_t wh2_parse_stream_desc(const uint8_t *data, size_t len, wh2_stream_desc_t *out);
wh2_parse_status_t wh2_parse_video(const uint8_t *data, size_t len, wh2_video_t *out);
wh2_parse_status_t wh2_parse_audio(const uint8_t *data, size_t len, wh2_audio_t *out);

#endif

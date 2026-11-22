/*
 * Wormhole - low-latency OBS network video/audio plugin
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Byte-accurate C implementation of the original Teleport wire format.
 * See PROTOCOL.md for the full specification. All integers are little-endian
 * and structs are serialized field-by-field, without C struct padding.
 */
#ifndef WORMHOLE_PROTOCOL_H
#define WORMHOLE_PROTOCOL_H

#include <stddef.h>
#include <stdint.h>

#define WH_HEADER_LEN 16u
#define WH_IMAGE_HEADER_LEN 88u
#define WH_WAVE_HEADER_LEN 16u
#define WH_DEFAULT_MAX_PAYLOAD (64u * 1024u * 1024u)

typedef enum {
    WH_KIND_JPEG = 0,
    WH_KIND_WAVE = 1,
    WH_KIND_ANJA = 2,
    WH_KIND_UNKNOWN = 3
} wh_kind_t;

typedef enum {
    WH_OK = 0,
    WH_ERR_EOF = 1,
    WH_ERR_INVALID_SIZE = 2,
    WH_ERR_TOO_LARGE = 3,
    WH_ERR_BUFFER_TOO_SMALL = 4,
    WH_ERR_NULL = 5
} wh_status_t;

typedef struct {
    uint8_t type[4];
    wh_kind_t kind;
    uint64_t timestamp;
    int32_t size;
} wh_header_t;

typedef struct {
    float color_matrix[16];
    float color_range_min[3];
    float color_range_max[3];
} wh_image_header_t;

typedef struct {
    int32_t format;
    int32_t sample_rate;
    int32_t speakers;
    int32_t frames;
} wh_wave_header_t;

typedef struct {
    wh_header_t header;
    wh_image_header_t image;
    wh_wave_header_t wave;
    uint8_t *payload;
    size_t payload_len;
} wh_packet_t;

/* Convert between the four wire bytes and the kind enum. */
wh_kind_t wh_kind_from_bytes(const uint8_t bytes[4]);
void wh_kind_to_bytes(wh_kind_t kind, uint8_t out[4]);
size_t wh_header_extra_len(wh_kind_t kind);

/* Initialize/free a packet. Decoded packets own their payload. */
void wh_packet_init(wh_packet_t *packet);
void wh_packet_free(wh_packet_t *packet);

/* Decode one packet starting at *offset; advances *offset by its wire length. */
wh_status_t wh_decode(const uint8_t *data, size_t len, size_t *offset,
                      wh_packet_t *out, size_t max_payload);

/* Encode one packet into out (cap bytes); writes the byte count to *written. */
wh_status_t wh_encode(const wh_packet_t *packet, uint8_t *out, size_t cap,
                      size_t *written);

/* Common 16-byte header encoder and type-specific header encoders. */
int wh_header_encode(uint8_t *out, size_t cap, const wh_header_t *header);
int wh_image_header_encode(uint8_t *out, size_t cap, const wh_image_header_t *image);
int wh_wave_header_encode(uint8_t *out, size_t cap, const wh_wave_header_t *wave);

/* Helpers matching the receiver rules documented in PROTOCOL.md. */
int wh_image_is_full_range(const wh_image_header_t *image);
int wh_wave_channels(int32_t speakers);
int wh_wave_bytes_per_sample(int32_t format);

#endif /* WORMHOLE_PROTOCOL_H */

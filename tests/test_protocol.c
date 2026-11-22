/*
 * Wormhole - low-latency OBS network video/audio plugin
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "wormhole/protocol.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_jpeg(wh_packet_t *packet)
{
    static const uint8_t payload[3] = {0xff, 0xd8, 0x01};
    wh_packet_init(packet);
    wh_kind_to_bytes(WH_KIND_JPEG, packet->header.type);
    packet->header.kind = WH_KIND_JPEG;
    packet->header.timestamp = 0x0102030405060708ULL;
    packet->header.size = (int32_t)sizeof(payload);
    packet->image.color_matrix[0] = 0.25f;
    packet->image.color_range_max[0] = 1.0f;
    packet->image.color_range_max[1] = 1.0f;
    packet->image.color_range_max[2] = 1.0f;
    packet->payload_len = sizeof(payload);
    packet->payload = (uint8_t *)malloc(sizeof(payload));
    assert(packet->payload != NULL);
    memcpy(packet->payload, payload, sizeof(payload));
}

static void test_header_layout(void)
{
    wh_packet_t packet;
    uint8_t buffer[256];
    size_t written = 0;

    make_jpeg(&packet);
    assert(wh_encode(&packet, buffer, sizeof(buffer), &written) == WH_OK);
    assert(written == WH_HEADER_LEN + WH_IMAGE_HEADER_LEN + 3u);
    assert(memcmp(buffer, "JPEG", 4) == 0);
    assert(buffer[4] == 0x08 && buffer[11] == 0x01);
    assert(buffer[12] == 0x03 && buffer[15] == 0x00);
    wh_packet_free(&packet);
    printf("ok: header layout\n");
}

static void test_round_trip_all_kinds(void)
{
    uint8_t buffer[512];
    uint8_t second[512];
    size_t written = 0;
    size_t second_written = 0;
    size_t offset = 0;
    wh_packet_t packet;
    wh_packet_t decoded;

    wh_packet_init(&packet);
    wh_kind_to_bytes(WH_KIND_WAVE, packet.header.type);
    packet.header.kind = WH_KIND_WAVE;
    packet.header.timestamp = 42u;
    packet.header.size = 4;
    packet.wave.format = 4;
    packet.wave.sample_rate = 48000;
    packet.wave.speakers = 2;
    packet.wave.frames = 1;
    packet.payload_len = 4u;
    packet.payload = (uint8_t *)malloc(4u);
    assert(packet.payload != NULL);
    memcpy(packet.payload, "\x01\x02\x03\x04", 4u);

    assert(wh_encode(&packet, buffer, sizeof(buffer), &written) == WH_OK);
    assert(wh_decode(buffer, written, &offset, &decoded, WH_DEFAULT_MAX_PAYLOAD) == WH_OK);
    assert(offset == written);
    assert(decoded.header.kind == WH_KIND_WAVE);
    assert(decoded.wave.sample_rate == 48000);
    assert(decoded.wave.speakers == 2);
    assert(decoded.payload_len == 4u);
    assert(memcmp(decoded.payload, packet.payload, 4u) == 0);
    assert(wh_encode(&decoded, second, sizeof(second), &second_written) == WH_OK);
    assert(second_written == written);
    assert(memcmp(buffer, second, written) == 0);
    wh_packet_free(&packet);
    wh_packet_free(&decoded);

    /* Unknown type bytes must survive a round trip unchanged. */
    wh_packet_init(&packet);
    packet.header.type[0] = 'X';
    packet.header.type[1] = 'Y';
    packet.header.type[2] = 'Z';
    packet.header.type[3] = '!';
    packet.header.kind = wh_kind_from_bytes(packet.header.type);
    packet.header.size = 2;
    packet.payload_len = 2u;
    packet.payload = (uint8_t *)malloc(2u);
    assert(packet.payload != NULL);
    packet.payload[0] = 9u;
    packet.payload[1] = 8u;
    assert(wh_encode(&packet, buffer, sizeof(buffer), &written) == WH_OK);
    offset = 0;
    assert(wh_decode(buffer, written, &offset, &decoded, WH_DEFAULT_MAX_PAYLOAD) == WH_OK);
    assert(decoded.header.kind == WH_KIND_UNKNOWN);
    assert(wh_encode(&decoded, second, sizeof(second), &second_written) == WH_OK);
    assert(memcmp(buffer, second, written) == 0);
    wh_packet_free(&packet);
    wh_packet_free(&decoded);
    printf("ok: round trip all kinds\n");
}

static void test_malformed_input(void)
{
    uint8_t buffer[64];
    size_t offset = 0;
    wh_packet_t packet;

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, "WAVE", 4);
    buffer[12] = 0xff;
    buffer[13] = 0xff;
    buffer[14] = 0xff;
    buffer[15] = 0xff; /* size = -1 */
    assert(wh_decode(buffer, sizeof(buffer), &offset, &packet, WH_DEFAULT_MAX_PAYLOAD) ==
           WH_ERR_INVALID_SIZE);

    memset(buffer, 0, sizeof(buffer));
    memcpy(buffer, "JPEG", 4);
    buffer[12] = 0x10;
    assert(wh_decode(buffer, 20u, &offset, &packet, WH_DEFAULT_MAX_PAYLOAD) == WH_ERR_EOF);
    assert(wh_decode(buffer, sizeof(buffer), &offset, &packet, 4u) == WH_ERR_TOO_LARGE);
    assert(wh_decode(NULL, sizeof(buffer), &offset, &packet, WH_DEFAULT_MAX_PAYLOAD) ==
           WH_ERR_NULL);
    printf("ok: malformed input\n");
}

static void test_helpers_and_multiple_packets(void)
{
    uint8_t stream[1024];
    size_t written = 0;
    size_t total = 0;
    size_t offset = 0;
    wh_packet_t first;
    wh_packet_t second;
    wh_packet_t decoded;
    int count = 0;

    make_jpeg(&first);
    assert(wh_encode(&first, stream, sizeof(stream), &written) == WH_OK);
    total += written;
    wh_packet_init(&second);
    wh_kind_to_bytes(WH_KIND_JPEG, second.header.type);
    second.header.kind = WH_KIND_JPEG;
    second.header.timestamp = 7u;
    second.header.size = 0;
    second.image.color_range_max[0] = 1.0f;
    assert(wh_encode(&second, stream + total, sizeof(stream) - total, &written) == WH_OK);
    total += written;

    while (offset < total) {
        assert(wh_decode(stream, total, &offset, &decoded, WH_DEFAULT_MAX_PAYLOAD) == WH_OK);
        ++count;
        wh_packet_free(&decoded);
    }
    assert(count == 2);
    assert(wh_image_is_full_range(&first.image) == 1);
    assert(wh_wave_channels(2) == 2);
    assert(wh_wave_channels(8) == 8);
    assert(wh_wave_bytes_per_sample(4) == 4);
    wh_packet_free(&first);
    printf("ok: helpers and multiple packets\n");
}

int main(void)
{
    test_header_layout();
    test_round_trip_all_kinds();
    test_malformed_input();
    test_helpers_and_multiple_packets();
    printf("ALL C TESTS PASSED\n");
    return 0;
}

/*
 * Wormhole - low-latency OBS network video/audio plugin
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Writes a deterministic Teleport-compatible capture using the C encoder.
 * Usage: capture_writer.exe <output.bin> [frames]
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "wormhole/protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    const char *path;
    int frames = 4;
    int frame;
    FILE *file;
    uint8_t buffer[512];
    wh_packet_t packet;

    if (argc < 2) {
        fprintf(stderr, "usage: %s <output.bin> [frames]\n", argv[0]);
        return 1;
    }
    path = argv[1];
    if (argc > 2) {
        frames = atoi(argv[2]);
        if (frames < 1)
            return 1;
    }

    file = fopen(path, "wb");
    if (file == NULL) {
        fprintf(stderr, "cannot open %s\n", path);
        return 1;
    }

    for (frame = 0; frame < frames; ++frame) {
        size_t written = 0;
        size_t i;

        wh_packet_init(&packet);
        wh_kind_to_bytes(WH_KIND_JPEG, packet.header.type);
        packet.header.kind = WH_KIND_JPEG;
        packet.header.timestamp = (uint64_t)frame * 33333333ULL;
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
        packet.header.timestamp = (uint64_t)frame * 33333333ULL;
        packet.header.size = 8;
        packet.wave.format = 2;
        packet.wave.sample_rate = 48000;
        packet.wave.speakers = 2;
        packet.wave.frames = 2;
        packet.payload_len = 8u;
        packet.payload = (uint8_t *)malloc(8u);
        if (packet.payload == NULL)
            return 1;
        for (i = 0; i < 8u; ++i)
            packet.payload[i] = (uint8_t)(frame * 8 + i);
        if (wh_encode(&packet, buffer, sizeof(buffer), &written) != WH_OK ||
            fwrite(buffer, 1, written, file) != written)
            return 1;
        wh_packet_free(&packet);
    }

    if (fclose(file) != 0)
        return 1;
    printf("wrote %d frames to %s\n", frames, path);
    return 0;
}

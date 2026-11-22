/*
 * Wormhole - all-C standalone Teleport-protocol reference peers.
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"
#include "stb/stb_image.h"

#include "wormhole/harness.h"
#include "wormhole/protocol.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET wh_socket_t;
#define WH_INVALID_SOCKET INVALID_SOCKET
#define wh_close_socket closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
typedef int wh_socket_t;
#define WH_INVALID_SOCKET (-1)
#define wh_close_socket close
#endif

static int parse_endpoint(const char *text, char *host, size_t host_cap,
                          unsigned short *port)
{
    const char *colon = strrchr(text, ':');
    size_t len;
    if (colon == NULL || colon == text)
        return 1;
    len = (size_t)(colon - text);
    if (len >= host_cap)
        return 1;
    memcpy(host, text, len);
    host[len] = '\0';
    *port = (unsigned short)strtoul(colon + 1, NULL, 10);
    return *port == 0u;
}

static wh_socket_t connect_to(const char *endpoint)
{
    char host[256];
    unsigned short port = 0;
    struct sockaddr_in address;
    wh_socket_t sock;
    if (parse_endpoint(endpoint, host, sizeof(host), &port) != 0)
        return WH_INVALID_SOCKET;
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == WH_INVALID_SOCKET)
        return sock;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1 ||
        connect(sock, (struct sockaddr *)&address, sizeof(address)) != 0) {
        wh_close_socket(sock);
        return WH_INVALID_SOCKET;
    }
    return sock;
}

static void fill_bars(unsigned char *rgb, unsigned width, unsigned height, unsigned frame)
{
    static const unsigned char colours[8][3] = {
        {235, 128, 128}, {210, 90, 240}, {170, 40, 220}, {128, 128, 128},
        {106, 170, 40}, {70, 220, 90}, {50, 240, 50}, {25, 128, 128}
    };
    unsigned marker = (frame * 8u) % width;
    unsigned row;
    unsigned col;
    for (row = 0; row < height; ++row) {
        for (col = 0; col < width; ++col) {
            unsigned bar = (col * 8u) / width;
            size_t at = ((size_t)row * width + col) * 3u;
            if (col >= marker && col < marker + 4u) {
                rgb[at] = 255; rgb[at + 1] = 255; rgb[at + 2] = 255;
            } else {
                rgb[at] = colours[bar][0];
                rgb[at + 1] = colours[bar][1];
                rgb[at + 2] = colours[bar][2];
            }
        }
    }
}

static int send_all(wh_socket_t sock, const unsigned char *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = send(sock, (const char *)(data + sent), (int)(len - sent), 0);
        if (n <= 0)
            return 1;
        sent += (size_t)n;
    }
    return 0;
}

int wh_cmd_ref_sender(const wh_config_t *config)
{
    wh_socket_t sock = connect_to(config->proxy_listen);
    unsigned char *rgb;
    unsigned width = config->media_width;
    unsigned height = config->media_height;
    unsigned char audio[4096];
    unsigned frame;
    char jpeg_path[WH_MAX_PATH + 64];
    unsigned char *wire = NULL;
    size_t wire_cap = 0;
    FILE *jpeg_file;
    long jpeg_len;

    if (sock == WH_INVALID_SOCKET) {
        fprintf(stderr, "error: cannot connect to %s\n", config->proxy_listen);
        return 1;
    }
    rgb = (unsigned char *)malloc((size_t)width * height * 3u);
    if (rgb == NULL)
        return 1;
    memset(audio, 0, sizeof(audio));
    snprintf(jpeg_path, sizeof(jpeg_path), "%s/ref-frame.jpg", config->output_dir);

    for (frame = 0; frame < config->media_frames; ++frame) {
        wh_packet_t packet;
        size_t written = 0;
        fill_bars(rgb, width, height, frame);
        if (stbi_write_jpg(jpeg_path, (int)width, (int)height, 3, rgb, 90) == 0) {
            free(rgb);
            return 1;
        }
        jpeg_file = fopen(jpeg_path, "rb");
        if (jpeg_file == NULL) {
            free(rgb);
            return 1;
        }
        fseek(jpeg_file, 0, SEEK_END);
        jpeg_len = ftell(jpeg_file);
        rewind(jpeg_file);
        {
            size_t needed = (size_t)jpeg_len + WH_HEADER_LEN + WH_IMAGE_HEADER_LEN + 64u;
            unsigned char *grown;
            if (needed < 8192u)
                needed = 8192u;
            if (needed > wire_cap) {
                grown = (unsigned char *)realloc(wire, needed);
                if (grown == NULL) {
                    fclose(jpeg_file);
                    free(rgb);
                    free(wire);
                    return 1;
                }
                wire = grown;
                wire_cap = needed;
            }
        }

        wh_packet_init(&packet);
        wh_kind_to_bytes(WH_KIND_JPEG, packet.header.type);
        packet.header.kind = WH_KIND_JPEG;
        packet.header.timestamp = (uint64_t)frame * 1000000000ULL / config->media_fps;
        packet.header.size = (int32_t)jpeg_len;
        packet.image.color_range_max[0] = 1.0f;
        packet.image.color_range_max[1] = 1.0f;
        packet.image.color_range_max[2] = 1.0f;
        packet.payload_len = (size_t)jpeg_len;
        packet.payload = (unsigned char *)malloc((size_t)jpeg_len);
        if (packet.payload == NULL ||
            fread(packet.payload, 1, (size_t)jpeg_len, jpeg_file) != (size_t)jpeg_len ||
            wh_encode(&packet, wire, wire_cap, &written) != WH_OK ||
            send_all(sock, wire, written) != 0) {
            wh_packet_free(&packet);
            fclose(jpeg_file);
            free(rgb);
            return 1;
        }
        wh_packet_free(&packet);
        fclose(jpeg_file);

        wh_packet_init(&packet);
        wh_kind_to_bytes(WH_KIND_WAVE, packet.header.type);
        packet.header.kind = WH_KIND_WAVE;
        packet.header.timestamp = (uint64_t)frame * 1000000000ULL / config->media_fps;
        packet.header.size = (int32_t)sizeof(audio);
        packet.wave.format = 4;
        packet.wave.sample_rate = (int32_t)config->media_sample_rate;
        packet.wave.speakers = 2;
        packet.wave.frames = (int32_t)(sizeof(audio) / (2u * 2u));
        packet.payload_len = sizeof(audio);
        packet.payload = audio;
        if (wh_encode(&packet, wire, wire_cap, &written) != WH_OK ||
            send_all(sock, wire, written) != 0) {
            packet.payload = NULL;
            wh_packet_free(&packet);
            free(rgb);
            free(wire);
            return 1;
        }
        packet.payload = NULL;
        wh_packet_free(&packet);
    }
    wh_close_socket(sock);
    free(rgb);
    free(wire);
    printf("ref-sender: sent %u frames to %s\n", config->media_frames, config->proxy_listen);
    return 0;
}

int wh_cmd_ref_receiver(const wh_config_t *config)
{
    char host[256];
    unsigned short port = 0;
    wh_socket_t listener;
    wh_socket_t client;
    struct sockaddr_in address;
    unsigned char *bytes = NULL;
    size_t length = 0;
    size_t capacity = 0;
    size_t offset = 0;
    unsigned char buffer[65536];
    size_t packets = 0;
    size_t video = 0;
    size_t audio = 0;
    size_t failures = 0;
    FILE *csv;

    if (parse_endpoint(config->proxy_target, host, sizeof(host), &port) != 0)
        return 1;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == WH_INVALID_SOCKET)
        return 1;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 1) != 0)
        return 1;
    printf("ref-receiver: listening on %s\n", config->proxy_target);
    client = accept(listener, NULL, NULL);
    if (client == WH_INVALID_SOCKET)
        return 1;

    for (;;) {
        int n = recv(client, (char *)buffer, (int)sizeof(buffer), 0);
        if (n <= 0)
            break;
        if (length + (size_t)n > capacity) {
            size_t next = capacity == 0 ? 65536u : capacity * 2u;
            unsigned char *grown;
            while (next < length + (size_t)n)
                next *= 2u;
            grown = (unsigned char *)realloc(bytes, next);
            if (grown == NULL)
                return 1;
            bytes = grown;
            capacity = next;
        }
        memcpy(bytes + length, buffer, (size_t)n);
        length += (size_t)n;
    }
    wh_close_socket(client);
    wh_close_socket(listener);

    csv = fopen(config->differential_output, "w");
    if (csv == NULL) {
        free(bytes);
        return 1;
    }
    {
        const char *header = "index,kind,timestamp_ns,payload_bytes,width,height,decoded\n";
        fwrite(header, 1, strlen(header), csv);
    }
    while (offset < length) {
        wh_packet_t packet;
        wh_status_t status = wh_decode(bytes, length, &offset, &packet, config->max_payload_bytes);
        const char *kind = "UNKNOWN";
        unsigned w = 0;
        unsigned h = 0;
        const char *decoded = "n/a";
        if (status != WH_OK)
            break;
        if (packet.header.kind == WH_KIND_JPEG) {
            int jw = 0;
            int jh = 0;
            int jc = 0;
            stbi_uc *pixels = stbi_load_from_memory(packet.payload, (int)packet.header.size,
                                                    &jw, &jh, &jc, 3);
            kind = "JPEG";
            ++video;
            if (pixels != NULL) {
                decoded = "ok";
                w = (unsigned)jw;
                h = (unsigned)jh;
                stbi_image_free(pixels);
            } else {
                decoded = "error";
                ++failures;
            }
        } else if (packet.header.kind == WH_KIND_WAVE) {
            kind = "WAVE";
            ++audio;
        }
        fprintf(csv, "%zu,%s,%llu,%d,%u,%u,%s\n", packets, kind,
                (unsigned long long)packet.header.timestamp, packet.header.size,
                w, h, decoded);
        ++packets;
        wh_packet_free(&packet);
    }
    fclose(csv);
    free(bytes);
    printf("ref-receiver: %zu packets (%zu video, %zu audio), %zu decode failures\n",
           packets, video, audio, failures);
    return failures == 0 ? 0 : 1;
}

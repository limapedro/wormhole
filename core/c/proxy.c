/*
 * Wormhole - all-C TCP capture/impairment proxy.
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Single-threaded select() relay: records both directions, injects fixed
 * delay, deterministic drops and an approximate bandwidth cap.
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#else
#define _POSIX_C_SOURCE 200809L
#endif

#include "wormhole/harness.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
typedef SOCKET wh_socket_t;
#define WH_INVALID_SOCKET INVALID_SOCKET
static void wh_sleep_ms(unsigned ms) { Sleep(ms); }
static void wh_sleep_us(unsigned long us) { Sleep((DWORD)((us + 999UL) / 1000UL)); }
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
typedef int wh_socket_t;
#define WH_INVALID_SOCKET (-1)
static void wh_sleep_ms(unsigned ms)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)(ms % 1000u) * 1000000L;
    nanosleep(&ts, NULL);
}
static void wh_sleep_us(unsigned long us)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(us / 1000000UL);
    ts.tv_nsec = (long)(us % 1000000UL) * 1000L;
    nanosleep(&ts, NULL);
}
#endif

static int parse_endpoint(const char *text, char *host, size_t host_cap,
                          unsigned short *port)
{
    const char *colon;
    size_t len;
    unsigned long value;

    if (text == NULL || host == NULL || port == NULL)
        return 1;
    colon = strrchr(text, ':');
    if (colon == NULL || colon == text)
        return 1;
    len = (size_t)(colon - text);
    if (len >= host_cap)
        return 1;
    memcpy(host, text, len);
    host[len] = '\0';
    value = strtoul(colon + 1, NULL, 10);
    if (value == 0u || value > 65535u)
        return 1;
    *port = (unsigned short)value;
    return 0;
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
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return WH_INVALID_SOCKET;
    }
    if (connect(sock, (struct sockaddr *)&address, sizeof(address)) != 0) {
#ifdef _WIN32
        closesocket(sock);
#else
        close(sock);
#endif
        return WH_INVALID_SOCKET;
    }
    return sock;
}

static int write_all(wh_socket_t sock, const uint8_t *data, size_t len)
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

static void record(FILE *file, const uint8_t *data, size_t len)
{
    if (file != NULL && len > 0u) {
        fwrite(data, 1, len, file);
        fflush(file);
    }
}

static void relay(wh_socket_t client, wh_socket_t upstream, const wh_config_t *cfg,
                  FILE *capture, FILE *capture_down)
{
    uint8_t buffer[65536];
    int client_open = 1;
    int upstream_open = 1;
    uint64_t drop_state = 0x9E3779B97F4A7C15ULL;

    while (client_open || upstream_open) {
        fd_set readfds;
        int ready;
        FD_ZERO(&readfds);
        if (client_open)
            FD_SET(client, &readfds);
        if (upstream_open)
            FD_SET(upstream, &readfds);
        ready = select((int)(client > upstream ? client : upstream) + 1, &readfds, NULL, NULL, NULL);
        if (ready <= 0)
            break;

        if (client_open && FD_ISSET(client, &readfds)) {
            int n = recv(client, (char *)buffer, (int)sizeof(buffer), 0);
            if (n <= 0) {
                client_open = 0;
                if (upstream_open)
                    shutdown(upstream, 1);
            } else {
                drop_state = drop_state * 6364136223846793005ULL + 1ULL;
                if ((unsigned)((drop_state >> 33) % 100u) >= cfg->drop_percent) {
                    if (cfg->delay_ms > 0u)
                        wh_sleep_ms(cfg->delay_ms);
                    if (cfg->bandwidth_bytes_per_sec > 0u) {
                        uint64_t micros = (uint64_t)n * 1000000ULL /
                                          cfg->bandwidth_bytes_per_sec;
                        if (micros > 1000000ULL)
                            micros = 1000000ULL;
                        wh_sleep_us((unsigned long)micros);
                    }
                    if (write_all(upstream, buffer, (size_t)n) == 0)
                        record(capture, buffer, (size_t)n);
                    else
                        upstream_open = 0;
                }
            }
        }

        if (upstream_open && FD_ISSET(upstream, &readfds)) {
            int n = recv(upstream, (char *)buffer, (int)sizeof(buffer), 0);
            if (n <= 0) {
                upstream_open = 0;
                if (client_open)
                    shutdown(client, 1);
            } else {
                record(capture_down, buffer, (size_t)n);
                if (write_all(client, buffer, (size_t)n) != 0)
                    client_open = 0;
            }
        }
    }
}

int wh_cmd_proxy(const wh_config_t *config)
{
    char host[256];
    unsigned short port = 0;
    wh_socket_t listener;
    struct sockaddr_in address;
    int reuse = 1;
    FILE *capture;
    FILE *capture_down;
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return 1;
#endif

    if (parse_endpoint(config->proxy_listen, host, sizeof(host), &port) != 0)
        return 1;
    capture = fopen(config->proxy_capture, "ab");
    capture_down = fopen(config->proxy_capture_down, "ab");
    if (capture == NULL || capture_down == NULL) {
        fprintf(stderr, "error: cannot open proxy capture files\n");
        if (capture != NULL) fclose(capture);
        if (capture_down != NULL) fclose(capture_down);
        return 1;
    }

    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == WH_INVALID_SOCKET)
        return 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(listener, (struct sockaddr *)&address, sizeof(address)) != 0 ||
        listen(listener, 16) != 0) {
        fprintf(stderr, "error: cannot bind %s\n", config->proxy_listen);
        return 1;
    }
    printf("proxy: listening on %s -> %s\n", config->proxy_listen, config->proxy_target);

    for (;;) {
        wh_socket_t client = accept(listener, NULL, NULL);
        wh_socket_t upstream;
        if (client == WH_INVALID_SOCKET)
            continue;
        upstream = connect_to(config->proxy_target);
        if (upstream == WH_INVALID_SOCKET) {
            fprintf(stderr, "proxy: target %s refused connection\n", config->proxy_target);
#ifdef _WIN32
            closesocket(client);
#else
            close(client);
#endif
            continue;
        }
        printf("proxy: connection established\n");
        relay(client, upstream, config, capture, capture_down);
#ifdef _WIN32
        closesocket(client);
        closesocket(upstream);
#else
        close(client);
        close(upstream);
#endif
        printf("proxy: connection ended\n");
    }
}

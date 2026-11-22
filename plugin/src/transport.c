/* Wormhole network transport. Copyright (C) 2026 Wormhole contributors. */
#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif
#include "wormhole/transport.h"
#include "wormhole/protocol_v2.h"

#include <string.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#define wh_close closesocket
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#define wh_close close
#endif

static int fill(struct sockaddr_in *addr, const char *host, uint16_t port)
{
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    return inet_pton(AF_INET, host ? host : "127.0.0.1", &addr->sin_addr) == 1;
}

int wh_net_listen(uint16_t port, wh_socket_t *out)
{
    struct sockaddr_in addr;
    int reuse = 1;
    wh_socket_t s;
    if (!out || !fill(&addr, "0.0.0.0", port))
        return 0;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == WH_INVALID_SOCKET)
        return 0;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuse, sizeof(reuse));
    if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) != 0 || listen(s, 4) != 0) {
        wh_close(s);
        return 0;
    }
    *out = s;
    return 1;
}

int wh_net_accept(wh_socket_t listener, wh_socket_t *out)
{
    wh_socket_t s = accept(listener, NULL, NULL);
    if (s == WH_INVALID_SOCKET)
        return 0;
    *out = s;
    return 1;
}

int wh_net_connect(const char *host, uint16_t port, wh_socket_t *out)
{
    struct sockaddr_in addr;
    wh_socket_t s;
    if (!out || !fill(&addr, host, port))
        return 0;
    s = socket(AF_INET, SOCK_STREAM, 0);
    if (s == WH_INVALID_SOCKET)
        return 0;
    if (connect(s, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        wh_close(s);
        return 0;
    }
    *out = s;
    return 1;
}

void wh_net_close(wh_socket_t sock)
{
    if (sock != WH_INVALID_SOCKET)
        wh_close(sock);
}

int wh_net_send(wh_socket_t sock, const uint8_t *data, size_t len)
{
    size_t sent = 0;
    while (sent < len) {
        int n = send(sock, (const char *)data + sent, (int)(len - sent), 0);
        if (n <= 0)
            return 0;
        sent += (size_t)n;
    }
    return 1;
}

static int recv_exact(wh_socket_t sock, uint8_t *buf, size_t len)
{
    size_t got = 0;
    while (got < len) {
        int n = recv(sock, (char *)buf + got, (int)(len - got), 0);
        if (n <= 0)
            return 0;
        got += (size_t)n;
    }
    return 1;
}

int wh_net_recv_frame(wh_socket_t sock, uint8_t *buf, size_t cap, size_t *len)
{
    uint32_t total = 0;
    int32_t teleport_size;
    size_t extra = 0;
    size_t at;

    if (!buf || !len || cap < 28)
        return 0;
    if (!recv_exact(sock, buf, 4))
        return 0;
    if (memcmp(buf, WH2_MAGIC, 4) == 0) {
        if (!recv_exact(sock, buf + 4, WH2_HEADER_LEN - 4))
            return 0;
        total = (uint32_t)buf[8] | ((uint32_t)buf[9] << 8) |
                ((uint32_t)buf[10] << 16) | ((uint32_t)buf[11] << 24);
        if (total < WH2_HEADER_LEN || total > cap)
            return 0;
        if (!recv_exact(sock, buf + WH2_HEADER_LEN, total - WH2_HEADER_LEN))
            return 0;
        *len = total;
        return 1;
    }

    /* Teleport-compatible packet: 16-byte header + type header + payload. */
    if (!recv_exact(sock, buf + 4, 12))
        return 0;
    teleport_size = (int32_t)((uint32_t)buf[12] | ((uint32_t)buf[13] << 8) |
                              ((uint32_t)buf[14] << 16) | ((uint32_t)buf[15] << 24));
    if (teleport_size < 0 || (uint32_t)teleport_size > cap)
        return 0;
    if (memcmp(buf, "JPEG", 4) == 0)
        extra = 88;
    else if (memcmp(buf, "WAVE", 4) == 0)
        extra = 16;
    at = 16 + extra;
    if (at + (size_t)teleport_size > cap)
        return 0;
    if (extra && !recv_exact(sock, buf + 16, extra))
        return 0;
    if (teleport_size && !recv_exact(sock, buf + at, (size_t)teleport_size))
        return 0;
    *len = at + (size_t)teleport_size;
    return 1;
}

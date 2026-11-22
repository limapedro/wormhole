/* Wormhole LAN discovery. Copyright (C) 2026 Wormhole contributors. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/discovery.h"
#include "wormhole/queue.h"
#include "wormhole/transport.h"

#include <obs-module.h>

#include <stdio.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

static wh_peer_t peers[WH_MAX_PEERS];
static wh_mutex_t peers_mutex;
static int peers_mutex_ready;
static volatile int sender_running;
static volatile int receiver_running;
static wh_thread_t sender_thread;
static wh_thread_t receiver_thread;
static wh_socket_t sender_sock = WH_INVALID_SOCKET;
static wh_socket_t receiver_sock = WH_INVALID_SOCKET;
static char sender_name[128];
static uint16_t sender_port;

static void ensure_mutex(void)
{
    if (!peers_mutex_ready) {
        wh_mutex_init(&peers_mutex);
        peers_mutex_ready = 1;
    }
}

static void remember_peer(const char *name, const char *ip, uint16_t port)
{
    size_t i;
    uint64_t now = (uint64_t)time(NULL);
    ensure_mutex();
    wh_mutex_lock(&peers_mutex);
    for (i = 0; i < WH_MAX_PEERS; ++i) {
        if (strcmp(peers[i].ip, ip) == 0 && peers[i].port == port) {
            snprintf(peers[i].name, sizeof(peers[i].name), "%s", name);
            peers[i].last_seen = now;
            wh_mutex_unlock(&peers_mutex);
            return;
        }
    }
    for (i = 0; i < WH_MAX_PEERS; ++i) {
        if (peers[i].last_seen == 0) {
            snprintf(peers[i].name, sizeof(peers[i].name), "%s", name);
            snprintf(peers[i].ip, sizeof(peers[i].ip), "%s", ip);
            peers[i].port = port;
            peers[i].last_seen = now;
            wh_mutex_unlock(&peers_mutex);
            blog(LOG_INFO, "[wormhole] DISCOVERED node '%s' at %s:%u", name, ip,
                 (unsigned)port);
            return;
        }
    }
    wh_mutex_unlock(&peers_mutex);
}

static void prune_peers(void)
{
    size_t i;
    uint64_t now = (uint64_t)time(NULL);
    ensure_mutex();
    wh_mutex_lock(&peers_mutex);
    for (i = 0; i < WH_MAX_PEERS; ++i) {
        if (peers[i].last_seen && now - peers[i].last_seen > 5u)
            memset(&peers[i], 0, sizeof(peers[i]));
    }
    wh_mutex_unlock(&peers_mutex);
}

size_t wh_discovery_peers(wh_peer_t *out, size_t cap)
{
    size_t i;
    size_t n = 0;
    ensure_mutex();
    wh_mutex_lock(&peers_mutex);
    for (i = 0; i < WH_MAX_PEERS && n < cap; ++i) {
        if (peers[i].last_seen) {
            out[n++] = peers[i];
        }
    }
    wh_mutex_unlock(&peers_mutex);
    return n;
}

static WH_THREAD_API discovery_sender_thread(WH_THREAD_ARG arg)
{
    struct sockaddr_in addr;
    char payload[320];
    int broadcast = 1;
    (void)arg;

    sender_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sender_sock == WH_INVALID_SOCKET)
        return WH_THREAD_RET;
    setsockopt(sender_sock, SOL_SOCKET, SO_BROADCAST, (const char *)&broadcast,
               sizeof(broadcast));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(WH_DISCOVERY_PORT);
    inet_pton(AF_INET, "255.255.255.255", &addr.sin_addr);

    while (sender_running) {
        snprintf(payload, sizeof(payload), "WORMHOLE2|%s|%u", sender_name,
                 (unsigned)sender_port);
        sendto(sender_sock, payload, (int)strlen(payload), 0,
               (struct sockaddr *)&addr, sizeof(addr));
        wh_sleep_ms(1000);
    }
    wh_net_close(sender_sock);
    sender_sock = WH_INVALID_SOCKET;
    return WH_THREAD_RET;
}

static WH_THREAD_API discovery_receiver_thread(WH_THREAD_ARG arg)
{
    struct sockaddr_in addr;
    char buf[512];
#ifdef _WIN32
    int timeout = 300;
#else
    struct timeval timeout = {0, 300000};
#endif
    (void)arg;

    receiver_sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver_sock == WH_INVALID_SOCKET)
        return WH_THREAD_RET;
#ifdef _WIN32
    setsockopt(receiver_sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&timeout,
               sizeof(timeout));
#else
    setsockopt(receiver_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(WH_DISCOVERY_PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(receiver_sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        wh_net_close(receiver_sock);
        receiver_sock = WH_INVALID_SOCKET;
        return WH_THREAD_RET;
    }

    while (receiver_running) {
        struct sockaddr_in from;
#ifdef _WIN32
        int from_len = sizeof(from);
#else
        socklen_t from_len = sizeof(from);
#endif
        int n = recvfrom(receiver_sock, buf, (int)sizeof(buf) - 1, 0,
                         (struct sockaddr *)&from, &from_len);
        char ip[64] = "";
        char name[128] = "";
        unsigned port = 0;
        if (n <= 0) {
            prune_peers();
            continue;
        }
        buf[n] = '\0';
        if (sscanf(buf, "WORMHOLE2|%127[^|]|%u", name, &port) != 2 || !port)
            continue;
        inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
        remember_peer(name, ip, (uint16_t)port);
        prune_peers();
    }
    wh_net_close(receiver_sock);
    receiver_sock = WH_INVALID_SOCKET;
    return WH_THREAD_RET;
}

int wh_discovery_sender_start(const char *name, uint16_t port)
{
    if (sender_running)
        return 1;
    snprintf(sender_name, sizeof(sender_name), "%s", name ? name : "Wormhole Sender");
    sender_port = port;
    sender_running = 1;
    return wh_thread_create(&sender_thread, discovery_sender_thread, NULL);
}

void wh_discovery_sender_stop(void)
{
    sender_running = 0;
    wh_net_close(sender_sock);
}

int wh_discovery_receiver_start(void)
{
    if (receiver_running)
        return 1;
    receiver_running = 1;
    return wh_thread_create(&receiver_thread, discovery_receiver_thread, NULL);
}

void wh_discovery_receiver_stop(void)
{
    receiver_running = 0;
    wh_net_close(receiver_sock);
}

/* Wormhole LAN discovery: broadcast sender identity, collect peers. */
#ifndef WORMHOLE_DISCOVERY_H
#define WORMHOLE_DISCOVERY_H

#include <stddef.h>
#include <stdint.h>

#define WH_DISCOVERY_PORT 18092u
#define WH_MAX_PEERS 32u

typedef struct {
    char name[128];
    char ip[64];
    uint16_t port;
    uint64_t last_seen;
} wh_peer_t;

int wh_discovery_sender_start(const char *name, uint16_t port);
void wh_discovery_sender_stop(void);

int wh_discovery_receiver_start(void);
void wh_discovery_receiver_stop(void);

/* Copy currently visible peers into out; returns the number written. */
size_t wh_discovery_peers(wh_peer_t *out, size_t cap);

#endif

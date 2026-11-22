/* Audio channels advertised by the sender over the video connection. */
#ifndef WORMHOLE_CHANNELS_H
#define WORMHOLE_CHANNELS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    char name[128];
    char host[64];
    uint16_t port;
} wh_channel_t;

/* Called by the video receiver when the sender's channel list arrives. */
void wh_channels_set(const char *host, const char *text);
/* Snapshot for UI property population; returns count written. */
size_t wh_channels_list(wh_channel_t *out, size_t cap);

#endif

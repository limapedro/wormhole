/* Shared registry of remote audio channels learned from the video connection. */
#include "wormhole/channels.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WH_MAX_CHANNELS 16

static wh_channel_t channels[WH_MAX_CHANNELS];
static int channel_count;

void wh_channels_set(const char *host, const char *text)
{
    int count = 0;
    const char *cursor = text;

    memset(channels, 0, sizeof(channels));
    channel_count = 0;
    if (!text || !host)
        return;

    while (cursor && *cursor && count < WH_MAX_CHANNELS) {
        const char *end = strchr(cursor, '\n');
        char line[256];
        size_t len = end ? (size_t)(end - cursor) : strlen(cursor);
        char *bar;

        if (len >= sizeof(line))
            len = sizeof(line) - 1;
        memcpy(line, cursor, len);
        line[len] = '\0';
        cursor = end ? end + 1 : NULL;

        bar = strchr(line, '|');
        if (!bar)
            continue;
        *bar = '\0';
        snprintf(channels[count].name, sizeof(channels[count].name), "%s", line);
        snprintf(channels[count].host, sizeof(channels[count].host), "%s", host);
        channels[count].port = (uint16_t)atoi(bar + 1);
        if (channels[count].port)
            ++count;
    }
    channel_count = count;
}

size_t wh_channels_list(wh_channel_t *out, size_t cap)
{
    size_t n = 0;
    size_t i;
    for (i = 0; i < (size_t)channel_count && n < cap; ++i)
        out[n++] = channels[i];
    return n;
}

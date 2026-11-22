/* Wormhole JPEG media helpers (libjpeg-turbo 3.2.0 SIMD). */
#ifndef WORMHOLE_MEDIA_H
#define WORMHOLE_MEDIA_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
    void *compress;
    void *decompress;
} wh_media_t;

int wh_media_init(wh_media_t *media);
void wh_media_free(wh_media_t *media);

/* YUV planes are tightly packed: Y = w*h, Cb/Cr depend on subsampling. */
int wh_media_encode(wh_media_t *media, const uint8_t *yuv, int width, int height,
                    int subsampling, int quality,
                    uint8_t **jpeg, size_t *jpeg_len);
void wh_media_free_jpeg(uint8_t *jpeg);

int wh_media_decode(wh_media_t *media, const uint8_t *jpeg, size_t jpeg_len,
                    uint8_t *yuv, int *width, int *height, int *subsampling);

#endif

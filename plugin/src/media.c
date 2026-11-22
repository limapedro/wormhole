/* Wormhole JPEG media helpers. Copyright (C) 2026 Wormhole contributors. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/media.h"

#include <stdlib.h>
#include <turbojpeg.h>

int wh_media_init(wh_media_t *media)
{
    if (!media)
        return 0;
    media->compress = tj3Init(TJINIT_COMPRESS);
    media->decompress = tj3Init(TJINIT_DECOMPRESS);
    if (!media->compress || !media->decompress) {
        wh_media_free(media);
        return 0;
    }
    tj3Set(media->compress, TJPARAM_NOREALLOC, 1);
    return 1;
}

void wh_media_free(wh_media_t *media)
{
    if (!media)
        return;
    if (media->compress) tj3Destroy(media->compress);
    if (media->decompress) tj3Destroy(media->decompress);
    media->compress = NULL;
    media->decompress = NULL;
}

int wh_media_encode(wh_media_t *media, const uint8_t *yuv, int width, int height,
                    int subsampling, int quality,
                    uint8_t **jpeg, size_t *jpeg_len)
{
    unsigned char *buffer = NULL;
    size_t size = 0;
    if (!media || !media->compress || !yuv || !jpeg || !jpeg_len)
        return 0;
    tj3Set(media->compress, TJPARAM_QUALITY, quality);
    tj3Set(media->compress, TJPARAM_SUBSAMP, subsampling);
    size = tj3JPEGBufSize(width, height, subsampling);
    buffer = (unsigned char *)malloc(size);
    if (!buffer)
        return 0;
    if (tj3CompressFromYUV8(media->compress, yuv, width, 1, height,
                            &buffer, &size) != 0) {
        free(buffer);
        return 0;
    }
    *jpeg = (uint8_t *)buffer;
    *jpeg_len = size;
    return 1;
}

void wh_media_free_jpeg(uint8_t *jpeg)
{
    free(jpeg);
}

int wh_media_decode(wh_media_t *media, const uint8_t *jpeg, size_t jpeg_len,
                    uint8_t *yuv, int *width, int *height, int *subsampling)
{
    int w = 0;
    int h = 0;
    int sub = 0;
    size_t need = 0;
    if (!media || !media->decompress || !jpeg || !yuv)
        return 0;
    if (tj3DecompressHeader(media->decompress, jpeg, jpeg_len) != 0)
        return 0;
    w = tj3Get(media->decompress, TJPARAM_JPEGWIDTH);
    h = tj3Get(media->decompress, TJPARAM_JPEGHEIGHT);
    sub = tj3Get(media->decompress, TJPARAM_SUBSAMP);
    need = (size_t)tj3YUVBufSize(w, 1, h, sub);
    (void)need; /* caller supplies a buffer sized for the negotiated format */
    if (tj3DecompressToYUV8(media->decompress, jpeg, jpeg_len, yuv, 1) != 0)
        return 0;
    if (width) *width = w;
    if (height) *height = h;
    if (subsampling) *subsampling = sub;
    return 1;
}

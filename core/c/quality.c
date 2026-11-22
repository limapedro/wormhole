/*
 * Wormhole - all-C image-quality analysis of captured JPEG frames.
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "wormhole/harness.h"
#include "wormhole/protocol.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int wh_cmd_quality(const wh_config_t *config)
{
    FILE *file = fopen(config->dissect_input, "rb");
    uint8_t *bytes;
    long length;
    size_t offset = 0;
    size_t frames = 0;
    unsigned width = 0;
    unsigned height = 0;
    double sum_r = 0.0;
    double sum_g = 0.0;
    double sum_b = 0.0;
    double sum_mse = 0.0;
    int use_expected = config->expected_r >= 0 && config->expected_g >= 0 &&
                       config->expected_b >= 0;
    FILE *report;

    if (file == NULL) {
        fprintf(stderr, "error: cannot read capture %s\n", config->dissect_input);
        return 1;
    }
    fseek(file, 0, SEEK_END);
    length = ftell(file);
    rewind(file);
    if (length <= 0) {
        fclose(file);
        return 1;
    }
    bytes = (uint8_t *)malloc((size_t)length);
    if (bytes == NULL || fread(bytes, 1, (size_t)length, file) != (size_t)length) {
        free(bytes);
        fclose(file);
        return 1;
    }
    fclose(file);

    while (offset < (size_t)length) {
        wh_packet_t packet;
        if (wh_decode(bytes, (size_t)length, &offset, &packet, config->max_payload_bytes) != WH_OK) {
            free(bytes);
            return 1;
        }
        if (packet.header.kind == WH_KIND_JPEG) {
            int w = 0;
            int h = 0;
            int channels = 0;
            stbi_uc *pixels = stbi_load_from_memory(packet.payload,
                                                    (int)packet.header.size,
                                                    &w, &h, &channels, 3);
            size_t count;
            size_t i;
            double r = 0.0;
            double g = 0.0;
            double b = 0.0;
            double mean[3];
            double target[3];
            double mse = 0.0;
            if (pixels == NULL || w <= 0 || h <= 0) {
                fprintf(stderr, "error: JPEG frame %zu failed to decode\n", frames);
                stbi_image_free(pixels);
                wh_packet_free(&packet);
                free(bytes);
                return 1;
            }
            count = (size_t)w * (size_t)h;
            for (i = 0; i < count; ++i) {
                r += (double)pixels[i * 3];
                g += (double)pixels[i * 3 + 1];
                b += (double)pixels[i * 3 + 2];
            }
            mean[0] = r / (double)count;
            mean[1] = g / (double)count;
            mean[2] = b / (double)count;
            if (use_expected) {
                target[0] = (double)config->expected_r;
                target[1] = (double)config->expected_g;
                target[2] = (double)config->expected_b;
            } else {
                target[0] = (double)(unsigned char)(mean[0] + 0.5);
                target[1] = (double)(unsigned char)(mean[1] + 0.5);
                target[2] = (double)(unsigned char)(mean[2] + 0.5);
            }
            for (i = 0; i < count; ++i) {
                double dr = (double)pixels[i * 3] - target[0];
                double dg = (double)pixels[i * 3 + 1] - target[1];
                double db = (double)pixels[i * 3 + 2] - target[2];
                mse += dr * dr + dg * dg + db * db;
            }
            mse /= (double)count * 3.0;
            sum_r += mean[0];
            sum_g += mean[1];
            sum_b += mean[2];
            sum_mse += mse;
            width = (unsigned)w;
            height = (unsigned)h;
            ++frames;
            stbi_image_free(pixels);
        }
        wh_packet_free(&packet);
    }
    free(bytes);
    if (frames == 0) {
        fprintf(stderr, "error: no JPEG frames found in capture\n");
        return 1;
    }

    {
        double mse = sum_mse / (double)frames;
        double psnr = mse <= 0.0 ? 120.0 : 10.0 * log10(255.0 * 255.0 / mse);
        if (psnr > 120.0)
            psnr = 120.0;
        report = fopen(config->quality_output, "w");
        if (report == NULL)
            return 1;
        fprintf(report, "{\n  \"frames_analyzed\": %zu,\n", frames);
        fprintf(report, "  \"width\": %u,\n  \"height\": %u,\n", width, height);
        fprintf(report, "  \"comparison\": \"%s\",\n",
                use_expected ? "configured expected colour" : "per-frame mean (uniformity)");
        fprintf(report, "  \"mean_r\": %.3f,\n  \"mean_g\": %.3f,\n  \"mean_b\": %.3f,\n",
                sum_r / (double)frames, sum_g / (double)frames, sum_b / (double)frames);
        fprintf(report, "  \"mse\": %.6f,\n  \"psnr_db\": %.2f\n}\n", mse, psnr);
        fclose(report);
        printf("quality: %zu frames %ux%u, mean RGB=(%.1f,%.1f,%.1f), MSE=%.3f, PSNR=%.2f dB\n",
               frames, width, height, sum_r / (double)frames, sum_g / (double)frames,
               sum_b / (double)frames, mse, psnr);
        printf("wrote %s\n", config->quality_output);
    }
    return 0;
}

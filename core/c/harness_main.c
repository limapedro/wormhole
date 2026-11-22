/*
 * Wormhole - all-C harness command line.
 * Only --config <path> and --help are accepted flags.
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include "wormhole/harness.h"

#include <stdio.h>
#include <string.h>

static void print_help(void)
{
    printf("wormhole-harness (C)\n");
    printf("Usage: wormhole-harness [--config <path>] <command>\n\n");
    printf("Commands:\n");
    printf("  check    Print the effective typed configuration\n");
    printf("  dissect  Decode a capture and verify byte-identical re-encoding\n");
    printf("  synth    Write a deterministic capture fixture\n");
    printf("  proxy    Transparent TCP capture/impairment proxy\n\n");
    printf("Options:\n");
    printf("  --config <path>  Typed TOML configuration file\n");
    printf("  --help           Print this message\n");
}

int main(int argc, char **argv)
{
    wh_config_t config;
    const char *config_path = NULL;
    const char *command = NULL;
    int i;

    wh_config_init(&config);
    for (i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--config") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "error: --config needs a path\n");
                return 1;
            }
            config_path = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0) {
            print_help();
            return 0;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "error: unknown flag %s (only --config and --help are allowed)\n", argv[i]);
            return 1;
        } else {
            command = argv[i];
        }
    }

    if (config_path != NULL && wh_config_load(&config, config_path) != 0)
        return 1;
    if (command == NULL) {
        print_help();
        return 1;
    }
    if (strcmp(command, "check") == 0)
        return wh_cmd_check(&config);
    if (strcmp(command, "dissect") == 0)
        return wh_cmd_dissect(&config);
    if (strcmp(command, "synth") == 0)
        return wh_cmd_synth(&config);
    if (strcmp(command, "proxy") == 0)
        return wh_cmd_proxy(&config);

    fprintf(stderr, "error: unknown command %s\n", command);
    return 1;
}

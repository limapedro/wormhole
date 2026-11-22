/*
 * Wormhole - low-latency OBS network video/audio plugin
 * Copyright (C) 2026 Wormhole contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Tools integration mirrors Teleport: a Tools menu item opens the settings
 * of a dummy source, whose update callback owns the output lifetime.
 */
#ifdef _WIN32
#include <winsock2.h>
#endif
#include <obs-module.h>
#include <obs-frontend-api.h>

#include <stdlib.h>
#include <string.h>

#define WH_VERSION "0.2.0"
#define WH_CONFIG "wormhole.json"

void wh_register_source(void);
void wh_register_audio_source(void);
void wh_register_output(void);
void wh_register_control(void);

OBS_DECLARE_MODULE()

static obs_source_t *wh_settings_source;

static obs_data_t *wh_load_settings(void)
{
    char *path = obs_module_get_config_path(obs_current_module(), WH_CONFIG);
    obs_data_t *settings = NULL;
    if (path) {
        settings = obs_data_create_from_json_file(path);
        bfree(path);
    }
    if (!settings)
        settings = obs_data_create();
    obs_data_set_default_bool(settings, "enabled", false);
    obs_data_set_default_int(settings, "port", 18090);
    obs_data_set_default_int(settings, "quality", 90);
    obs_data_set_default_int(settings, "tracks", 2);
    return settings;
}

static void wh_save_settings(obs_data_t *settings)
{
    char *path = obs_module_get_config_path(obs_current_module(), WH_CONFIG);
    if (!path)
        return;
    obs_data_save_json_safe(settings, path, "tmp", "bak");
    bfree(path);
}

static bool wh_find_receiver(void *param, obs_source_t *source)
{
    obs_source_t **out = param;
    const char *id = obs_source_get_id(source);
    if (id && strcmp(id, "wormhole-source") == 0) {
        *out = obs_source_get_ref(source);
        return false;
    }
    return true;
}

/* Server = gaming PC sending its output. Receiver = streaming PC mirroring it. */
static void wh_open_server(void *data)
{
    (void)data;
    if (wh_settings_source)
        obs_frontend_open_source_properties(wh_settings_source);
}

static void wh_open_receiver(void *data)
{
    obs_source_t *receiver = NULL;
    (void)data;
    obs_enum_sources(wh_find_receiver, &receiver);
    if (!receiver) {
        obs_source_t *scene_src = obs_frontend_get_current_scene();
        receiver = obs_source_create("wormhole-source", "Wormhole", NULL, NULL);
        if (receiver && scene_src) {
            obs_scene_t *scene = obs_scene_from_source(scene_src);
            if (scene)
                obs_scene_add(scene, receiver);
        }
        if (scene_src)
            obs_source_release(scene_src);
    }
    if (receiver) {
        obs_frontend_open_source_properties(receiver);
        obs_source_release(receiver);
    }
}

static void wh_frontend_event(enum obs_frontend_event event, void *data)
{
    (void)data;
    if (event == OBS_FRONTEND_EVENT_FINISHED_LOADING) {
        obs_data_t *settings = wh_load_settings();
        wh_settings_source = obs_source_create("wormhole-control", "Wormhole", settings, NULL);
        obs_data_release(settings);
        obs_frontend_add_tools_menu_item("Wormhole - Host Game PC...", wh_open_server, NULL);
        obs_frontend_add_tools_menu_item("Wormhole - Mirror Game PC...", wh_open_receiver, NULL);
        blog(LOG_INFO, "[wormhole] Tools menu installed (Host / Mirror)");
    } else if (event == OBS_FRONTEND_EVENT_EXIT && wh_settings_source) {
        obs_data_t *settings = obs_source_get_settings(wh_settings_source);
        wh_save_settings(settings);
        obs_data_release(settings);
    }
}

bool obs_module_load(void)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
        return false;
#endif
    blog(LOG_INFO, "[wormhole] version %s", WH_VERSION);
    wh_register_source();
    wh_register_audio_source();
    wh_register_output();
    wh_register_control();
    obs_frontend_add_event_callback(wh_frontend_event, NULL);
    return true;
}

void obs_module_unload(void)
{
    if (wh_settings_source) {
        obs_source_release(wh_settings_source);
        wh_settings_source = NULL;
    }
}

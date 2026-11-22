/* Wormhole sender settings + LAN announcement. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/discovery.h"
#include "wormhole/transport.h"

#include <obs-module.h>

#include <stdio.h>
#include <string.h>

static obs_output_t *wh_output;
static char wh_current_name[128] = "Wormhole Sender";
static uint16_t wh_current_port = 18090;

static const char *wh_control_name(void *type_data)
{
    (void)type_data;
    return "WORMHOLE // SENDER";
}

static void wh_default_name(char *dst, size_t cap)
{
    char host[128] = "";
    if (gethostname(host, (int)sizeof(host) - 1) != 0 || !host[0])
        snprintf(dst, cap, "WORMHOLE-SENDER");
    else
        snprintf(dst, cap, "%s", host);
}

static void wh_control_defaults(obs_data_t *settings)
{
    char name[128];
    wh_default_name(name, sizeof(name));
    obs_data_set_default_bool(settings, "enabled", false);
    obs_data_set_default_string(settings, "name", name);
    obs_data_set_default_int(settings, "port", 18090);
    obs_data_set_default_int(settings, "quality", 90);
}

static bool wh_status_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
    (void)props; (void)p; (void)data;
    blog(LOG_INFO, "[wormhole] === SENDER STATUS ===");
    blog(LOG_INFO, "[wormhole] identity   : %s", wh_current_name);
    blog(LOG_INFO, "[wormhole] discovery  : UDP %u / media TCP %u",
         (unsigned)WH_DISCOVERY_PORT, (unsigned)wh_current_port);
    blog(LOG_INFO, "[wormhole] hint: the IDENTIFIER is broadcast to every receiver on the LAN");
    return false;
}

static obs_properties_t *wh_control_properties(void *data)
{
    (void)data;
    obs_properties_t *props = obs_properties_create();
    obs_property_t *p;

    p = obs_properties_add_text(props, "banner",
        "  ______________________________________________________\n"
        " |  W O R M H O L E   //   N E U R A L   U P L I N K   |\n"
        " |  v0.2.0  ::  low-latency A/V transport             |\n"
        " |____________________________________________________|\n",
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_WARNING);

    p = obs_properties_add_text(props, "sec1",
        " ──[ 01 // TRANSPORT ]─────────────────────────────────", OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_NORMAL);
    obs_properties_add_bool(props, "enabled", "> ENGAGE UPLINK");
    obs_properties_add_int(props, "port", "> TCP PORT", 1024, 65535, 1);
    obs_properties_add_int_slider(props, "quality", "> JPEG QUALITY", 1, 100, 1);

    p = obs_properties_add_text(props, "sec2",
        " ──[ 02 // IDENTITY ]──────────────────────────────────", OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_NORMAL);
    p = obs_properties_add_text(props, "name", "> IDENTIFIER", OBS_TEXT_DEFAULT);
    obs_property_set_long_description(p,
        "This name is broadcast over UDP to every receiver on the LAN. "
        "Give each machine a unique name (e.g. GAMING-PC-NEO).");

    p = obs_properties_add_text(props, "sec3",
        " ──[ 03 // DIAGNOSTICS ]───────────────────────────────", OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_NORMAL);
    obs_properties_add_button2(props, "status", "> PRINT STATUS TO LOG", wh_status_clicked, NULL);

    p = obs_properties_add_text(props, "footer",
        " [ NET: UDP 18092 discovery / TCP 18090 media ]   [ CYBERPUNK BUILD ]",
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_WARNING);
    return props;
}

static void wh_control_update(void *data, obs_data_t *settings);

static void *wh_control_create(obs_data_t *settings, obs_source_t *source)
{
    (void)source;
    wh_control_update(NULL, settings);
    return (void *)1;
}

static void wh_control_destroy(void *data)
{
    (void)data;
    wh_discovery_sender_stop();
}

static void wh_control_update(void *data, obs_data_t *settings)
{
    (void)data;
    bool enabled = obs_data_get_bool(settings, "enabled");
    const char *name = obs_data_get_string(settings, "name");
    int port = (int)obs_data_get_int(settings, "port");

    wh_discovery_sender_stop();
    if (wh_output && obs_output_active(wh_output))
        obs_output_stop(wh_output);
    if (wh_output) {
        obs_output_release(wh_output);
        wh_output = NULL;
    }
    if (!enabled)
        return;
    if (!name || !*name) {
        char generated[128];
        wh_default_name(generated, sizeof(generated));
        obs_data_set_string(settings, "name", generated);
        name = generated;
    }
    snprintf(wh_current_name, sizeof(wh_current_name), "%s", name);
    wh_current_port = (uint16_t)port;
    wh_output = obs_output_create("wormhole-output", "Wormhole Sender", settings, NULL);
    if (!wh_output) {
        blog(LOG_ERROR, "[wormhole] cannot create output");
        return;
    }
    if (obs_output_start(wh_output)) {
        wh_discovery_sender_start(name, (uint16_t)port);
        blog(LOG_INFO, "[wormhole] uplink engaged as '%s' on port %d", name, port);
    } else {
        blog(LOG_ERROR, "[wormhole] output failed to start");
    }
}

static struct obs_source_info wh_control_info = {
    .id = "wormhole-control",
    .type = OBS_SOURCE_TYPE_INPUT,
    .output_flags = 0,
    .get_name = wh_control_name,
    .create = wh_control_create,
    .destroy = wh_control_destroy,
    .get_defaults = wh_control_defaults,
    .get_properties = wh_control_properties,
    .update = wh_control_update,
};

void wh_register_control(void)
{
    obs_register_source(&wh_control_info);
}

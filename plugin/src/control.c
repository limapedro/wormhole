/* Wormhole output control source: exposes output settings in OBS's UI. */
#if defined(_MSC_VER)
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "wormhole/transport.h"

#include <obs-module.h>

static obs_output_t *wh_output;

static const char *wh_control_name(void *type_data)
{
    (void)type_data;
    return "Wormhole Sender";
}

static void wh_control_defaults(obs_data_t *settings)
{
    obs_data_set_default_bool(settings, "enabled", false);
    obs_data_set_default_int(settings, "port", 18090);
    obs_data_set_default_int(settings, "quality", 90);
}

static obs_properties_t *wh_control_properties(void *data)
{
    (void)data;
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_bool(props, "enabled", "Wormhole Enabled");
    obs_properties_add_int(props, "port", "TCP Port", 1024, 65535, 1);
    obs_properties_add_int_slider(props, "quality", "JPEG Quality", 1, 100, 1);
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
}

static void wh_control_update(void *data, obs_data_t *settings)
{
    (void)data;
    bool enabled = obs_data_get_bool(settings, "enabled");
    if (wh_output && obs_output_active(wh_output))
        obs_output_stop(wh_output);
    if (wh_output) {
        obs_output_release(wh_output);
        wh_output = NULL;
    }
    if (!enabled)
        return;
    wh_output = obs_output_create("wormhole-output", "Wormhole Sender", settings, NULL);
    if (!wh_output) {
        blog(LOG_ERROR, "[wormhole] cannot create output");
        return;
    }
    if (obs_output_start(wh_output))
        blog(LOG_INFO, "[wormhole] output enabled via control source");
    else
        blog(LOG_ERROR, "[wormhole] output failed to start");
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

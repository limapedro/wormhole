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
static char wh_current_mode[16] = "server";

static const char *wh_control_name(void *type_data)
{
    (void)type_data;
    return "WORMHOLE // CONTROL PANEL";
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
    obs_data_set_default_int(settings, "tracks", 2);
    obs_data_set_default_string(settings, "mode", "server");
    obs_data_set_default_string(settings, "client_host", "127.0.0.1");
    obs_data_set_default_int(settings, "client_port", 18090);
}

/* Mode switch: OBS's frontend API only supports flat Tools entries, so the
 * Server/Client choice lives in a combobox at the top of this one panel. */
static bool wh_mode_changed(void *priv, obs_properties_t *props,
                            obs_property_t *property, obs_data_t *settings)
{
    const char *mode = obs_data_get_string(settings, "mode");
    bool server = !mode || strcmp(mode, "client") != 0;
    const char *server_props[] = {
        "sec_s1", "enabled", "port", "tracks", "quality",
        "sec_s2", "name", "sec_s3", "status", NULL
    };
    const char *client_props[] = {
        "sec_c1", "peer", "client_host", "client_port",
        "sec_c2", "client_info", NULL
    };
    int i;
    (void)priv; (void)property;
    for (i = 0; server_props[i]; ++i) {
        obs_property_t *p = obs_properties_get(props, server_props[i]);
        if (p) obs_property_set_visible(p, server);
    }
    for (i = 0; client_props[i]; ++i) {
        obs_property_t *p = obs_properties_get(props, client_props[i]);
        if (p) obs_property_set_visible(p, !server);
    }
    return true;
}

static bool wh_status_clicked(obs_properties_t *props, obs_property_t *p, void *data)
{
    (void)props; (void)p; (void)data;
    blog(LOG_INFO, "[wormhole] === SENDER STATUS ===");
    blog(LOG_INFO, "[wormhole] identity   : %s", wh_current_name);
    blog(LOG_INFO, "[wormhole] discovery  : UDP %u / media TCP %u",
         (unsigned)WH_DISCOVERY_PORT, (unsigned)wh_current_port);
    blog(LOG_INFO, "[wormhole] obs_video_active=%d (0 means Output Mode is editable)",
         (int)obs_video_active());
    return false;
}

static obs_properties_t *wh_control_properties(void *data)
{
    obs_properties_t *props = obs_properties_create();
    obs_property_t *p;
    wh_peer_t found[WH_MAX_PEERS];
    size_t count;
    size_t i;

    (void)data;
    p = obs_properties_add_text(props, "banner",
        "  ______________________________________________________\n"
        " | W O R M H O L E  //  C O N T R O L   P A N E L        |\n"
        " |  v0.2.0  ::  one video signal, N audio channels       |\n"
        " |____________________________________________________|\n",
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_WARNING);

    p = obs_properties_add_list(props, "mode", "> MODE", OBS_COMBO_TYPE_LIST,
                                OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(p, "SERVER - host the game PC", "server");
    obs_property_list_add_string(p, "CLIENT - mirror a game PC", "client");
    obs_property_set_long_description(p,
        "SERVER sends this OBS's output. CLIENT mirrors a remote game PC while "
        "keeping local mic/webcam sources.");
    obs_property_set_modified_callback2(p, wh_mode_changed, NULL);

    /* ---------- SERVER section ---------- */
    p = obs_properties_add_text(props, "sec_s1",
        " ──[ SERVER // TRANSPORT ]────────────────────────────", OBS_TEXT_INFO);
    obs_properties_add_bool(props, "enabled", "> ENGAGE UPLINK");
    obs_properties_add_int(props, "port", "> TCP PORT", 1024, 65535, 1);
    obs_properties_add_int(props, "tracks", "> AUDIO TRACKS (1-6)", 1, 6, 1);
    obs_properties_add_int_slider(props, "quality", "> JPEG QUALITY", 1, 100, 1);

    p = obs_properties_add_text(props, "sec_s2",
        " ──[ SERVER // IDENTITY ]─────────────────────────────", OBS_TEXT_INFO);
    p = obs_properties_add_text(props, "name", "> IDENTIFIER", OBS_TEXT_DEFAULT);
    obs_property_set_long_description(p,
        "Broadcast to every client on the LAN; shown in their node list.");

    p = obs_properties_add_text(props, "sec_s3",
        " ──[ SERVER // DIAGNOSTICS ]──────────────────────────", OBS_TEXT_INFO);
    obs_properties_add_button2(props, "status", "> PRINT STATUS TO LOG", wh_status_clicked, NULL);

    /* ---------- CLIENT section ---------- */
    p = obs_properties_add_text(props, "sec_c1",
        " ──[ CLIENT // GAME PC ]──────────────────────────────", OBS_TEXT_INFO);
    count = wh_discovery_peers(found, WH_MAX_PEERS);
    p = obs_properties_add_list(props, "peer", "> DISCOVERED NODES",
                                OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
    obs_property_list_add_string(p, "- MANUAL ENTRY -", "");
    for (i = 0; i < count; ++i) {
        char key[160];
        char label[256];
        snprintf(key, sizeof(key), "%s:%u", found[i].ip, (unsigned)found[i].port);
        snprintf(label, sizeof(label), "%s  /  %s", found[i].name, key);
        obs_property_list_add_string(p, label, key);
    }
    obs_properties_add_text(props, "client_host", "> HOST / IP", OBS_TEXT_DEFAULT);
    obs_properties_add_int(props, "client_port", "> TCP PORT", 1024, 65535, 1);
    p = obs_properties_add_text(props, "client_info",
        "Select a discovered node or enter an IP/port. The mirror reconnects "
        "automatically; a new mirrored scene (WH <scene>) keeps your local "
        "mic/webcam sources.", OBS_TEXT_INFO);

    p = obs_properties_add_text(props, "footer",
        " [ UDP 18092 discovery / TCP 18090 media ]   [ CYBERPUNK BUILD ]",
        OBS_TEXT_INFO);
    obs_property_text_set_info_type(p, OBS_TEXT_INFO_WARNING);
    {
        const char *names_s[] = {"sec_s1","enabled","port","tracks","quality","sec_s2","name","sec_s3","status",NULL};
        const char *names_c[] = {"sec_c1","peer","client_host","client_port","sec_c2","client_info",NULL};
        bool server = strcmp(wh_current_mode, "client") != 0;
        int j;
        for (j = 0; names_s[j]; ++j) {
            obs_property_t *q = obs_properties_get(props, names_s[j]);
            if (q) obs_property_set_visible(q, server);
        }
        for (j = 0; names_c[j]; ++j) {
            obs_property_t *q = obs_properties_get(props, names_c[j]);
            if (q) obs_property_set_visible(q, !server);
        }
    }
    return props;
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

/* Client mode writes the mirror target into the existing receiver source. */
static void wh_apply_client_settings(obs_data_t *settings)
{
    obs_source_t *rx = NULL;
    obs_data_t *rx_settings;
    const char *peer = obs_data_get_string(settings, "peer");
    const char *host = obs_data_get_string(settings, "client_host");
    int port = (int)obs_data_get_int(settings, "client_port");

    obs_enum_sources(wh_find_receiver, &rx);
    if (!rx) {
        blog(LOG_WARNING, "[wormhole] no receiver source to configure");
        return;
    }
    rx_settings = obs_source_get_settings(rx);
    if (peer && *peer) {
        char ip[96] = "";
        unsigned p = 0;
        if (sscanf(peer, "%95[^:]:%u", ip, &p) == 2 && p) {
            obs_data_set_string(rx_settings, "peer", ip);
            obs_data_set_int(rx_settings, "port", (int)p);
        }
    } else {
        obs_data_set_string(rx_settings, "peer", "");
        obs_data_set_string(rx_settings, "host", host ? host : "127.0.0.1");
        obs_data_set_int(rx_settings, "port", port ? port : 18090);
    }
    blog(LOG_INFO, "[wormhole] client target set: host=%s port=%d",
         obs_data_get_string(rx_settings, "host"),
         (int)obs_data_get_int(rx_settings, "port"));
    obs_data_release(rx_settings);
    obs_source_release(rx);
}

static void wh_control_update(void *data, obs_data_t *settings);

static void *wh_control_create(obs_data_t *settings, obs_source_t *source)
{
    (void)source;
    wh_control_update(NULL, settings);
    return (void *)1;
}

/* Tear down the current uplink. Safe to call repeatedly and from destroy. */
static void wh_control_shutdown(void)
{
    wh_discovery_sender_stop();
    if (wh_output) {
        /* OBS clears its "active" flag asynchronously, so testing
         * obs_output_active() here could skip the stop callback and let
         * obs_output_release() invoke it later against a freed thread
         * handle. Stop unconditionally; the output stop is idempotent. */
        obs_output_stop(wh_output);
        obs_output_release(wh_output);
        wh_output = NULL;
    }
}

static void wh_control_destroy(void *data)
{
    (void)data;
    wh_control_shutdown();
}

static void wh_control_update(void *data, obs_data_t *settings)
{
    (void)data;
    {
        const char *mode = obs_data_get_string(settings, "mode");
        snprintf(wh_current_mode, sizeof(wh_current_mode), "%s",
                 mode ? mode : "server");
    }
    bool enabled = obs_data_get_bool(settings, "enabled");
    const char *name = obs_data_get_string(settings, "name");
    int port = (int)obs_data_get_int(settings, "port");

    wh_control_shutdown();
    {
        const char *mode = obs_data_get_string(settings, "mode");
        if (mode && strcmp(mode, "client") == 0) {
            wh_apply_client_settings(settings);
            return;
        }
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

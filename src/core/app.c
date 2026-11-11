#include "app.h"
#include "profile.h"

#include <stdio.h>
#include <string.h>

static void set_message(ga_app *app, const char *msg)
{
    snprintf(app->message, sizeof(app->message), "%s", msg);
    app->message_seq++;
    if (app->notify)
        app->notify(msg);
}

void ga_app_init(ga_app *app, const char *profile_dir, const char *platform, int simulated)
{
    memset(app, 0, sizeof(*app));
    snprintf(app->profile_dir, sizeof(app->profile_dir), "%s", profile_dir);
    snprintf(app->platform, sizeof(app->platform), "%s", platform);
    app->simulated = simulated;
    ga_app_set_title(app, GA_DEFAULT_PROFILE);
}

void ga_app_set_title(ga_app *app, const char *title)
{
    char safe[64];
    ga_title_sanitize(title, safe, sizeof(safe));
    if (strcmp(safe, app->title) == 0)
        return;

    snprintf(app->title, sizeof(app->title), "%s", safe);
    app->title_has_profile = ga_profile_load(app->profile_dir, safe, &app->cfg);
    ga_state_reset(&app->st);

    char msg[128];
    if (strcmp(safe, GA_DEFAULT_PROFILE) == 0)
        snprintf(msg, sizeof(msg), "Gyro Aim: perfil padrao");
    else if (app->title_has_profile)
        snprintf(msg, sizeof(msg), "Gyro Aim: perfil de %s carregado", safe);
    else
        snprintf(msg, sizeof(msg), "Gyro Aim: %s sem perfil, usando o padrao", safe);
    set_message(app, msg);
}

static void save(ga_app *app)
{
    if (ga_profile_save(app->profile_dir, app->title, &app->cfg) == 0)
        app->title_has_profile = 1;
}

int ga_app_process(ga_app *app, const ga_input *in)
{
    app->last_in = *in;
    int evt = ga_update(&app->cfg, &app->st, in, &app->out);
    if (evt) {
        char msg[128];
        if (evt & GA_EVT_ENABLED)
            snprintf(msg, sizeof(msg), "Gyro Aim %s", app->cfg.enabled ? "LIGADO" : "DESLIGADO");
        else if (evt & GA_EVT_SENS)
            snprintf(msg, sizeof(msg), "Sensibilidade: %.1f", app->cfg.sensitivity);
        else
            snprintf(msg, sizeof(msg), "Anti-deadzone: %.0f%%", app->cfg.anti_deadzone * 100.0f);
        save(app);
        set_message(app, msg);
    }
    return evt;
}

int ga_app_set(ga_app *app, const char *key, const char *value)
{
    if (ga_config_set(&app->cfg, key, value) != 0)
        return -1;
    save(app);
    return 0;
}

int ga_app_save_as_default(ga_app *app)
{
    int r = ga_profile_save(app->profile_dir, GA_DEFAULT_PROFILE, &app->cfg);
    if (r == 0)
        set_message(app, "Gyro Aim: salvo como perfil padrao");
    return r;
}

/* Escapa só o necessário para os textos que geramos (sem controle/unicode estranho). */
static void json_str(char *dst, size_t n, const char *s)
{
    size_t j = 0;
    for (; *s && j + 2 < n; s++) {
        if (*s == '"' || *s == '\\') dst[j++] = '\\';
        dst[j++] = (unsigned char)*s < 0x20 ? ' ' : *s;
    }
    dst[j] = '\0';
}

size_t ga_app_state_json(const ga_app *app, char *buf, size_t n)
{
    char msg[256], title[128];
    json_str(msg, sizeof(msg), app->message);
    json_str(title, sizeof(title), app->title);

    size_t len = (size_t)snprintf(buf, n,
        "{\"platform\":\"%s\",\"simulated\":%d,\"title\":\"%s\",\"has_profile\":%d,"
        "\"message\":\"%s\",\"message_seq\":%u,"
        "\"config\":{\"enabled\":%d,\"activation\":\"%s\",\"toggle_mode\":%d,"
        "\"sensitivity\":%.2f,\"anti_deadzone\":%.2f,\"vertical_ratio\":%.2f,"
        "\"smoothing\":%.2f,\"gyro_deadzone\":%.2f,\"invert_x\":%d,\"invert_y\":%d},"
        "\"live\":{\"active\":%d,\"rx\":%.3f,\"ry\":%.3f,\"pitch\":%.1f,\"yaw\":%.1f,\"buttons\":%u},"
        "\"buttons\":[",
        app->platform, app->simulated, title, app->title_has_profile, msg, app->message_seq,
        app->cfg.enabled, ga_button_id(app->cfg.activation), app->cfg.toggle_mode,
        app->cfg.sensitivity, app->cfg.anti_deadzone, app->cfg.vertical_ratio,
        app->cfg.smoothing, app->cfg.gyro_deadzone, app->cfg.invert_x, app->cfg.invert_y,
        app->out.gyro_active, app->out.rx, app->out.ry,
        app->last_in.pitch_dps, app->last_in.yaw_dps, app->last_in.buttons);

    for (int b = 1; b < GA_BTN_COUNT && len < n; b++)
        len += (size_t)snprintf(buf + len, n - len, "%s{\"id\":\"%s\",\"label\":\"%s\"}",
                                b > 1 ? "," : "", ga_button_id((ga_button)b),
                                ga_button_label((ga_button)b));
    if (len < n)
        len += (size_t)snprintf(buf + len, n - len, "]}");
    return len < n ? len : n - 1;
}

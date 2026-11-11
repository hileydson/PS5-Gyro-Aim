#include "app.h"
#include "profile.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static void message(ga_app *a, const char *s)
{
    snprintf(a->message, sizeof(a->message), "%s", s);
    a->message_seq++;
    if (a->notify) a->notify(s);
}

void ga_app_init(ga_app *a, const char *dir, const char *platform, int simulated)
{
    memset(a, 0, sizeof(*a));
    snprintf(a->profile_dir, sizeof(a->profile_dir), "%s", dir);
    snprintf(a->platform, sizeof(a->platform), "%s", platform);
    a->simulated = simulated;
    ga_app_set_title(a, GA_DEFAULT_PROFILE);
}

void ga_app_set_title(ga_app *a, const char *title)
{
    char safe[64], msg[256];
    ga_title_sanitize(title, safe, sizeof(safe));
    if (!strcmp(safe, a->title)) return;
    snprintf(a->title, sizeof(a->title), "%s", safe);
    a->title_has_profile = ga_profile_load(a->profile_dir, safe, &a->cfg);
    a->title_name[0] = 0;
    snprintf(a->title_source, sizeof(a->title_source), "%s", a->simulated ? "manual" : "console");
    ga_state_reset(&a->st);
    memset(&a->out, 0, sizeof(a->out));
    a->can_undo = a->testing = a->calibrating = a->paused = a->save_error = 0;
    a->revision++;
    snprintf(msg, sizeof(msg), "Gyro Aim: %s · %s. Painel: guia e ajustes; Touchpad + Quadrado: ajuda.",
             safe, a->title_has_profile ? "perfil carregado" : "usando padrão");
    message(a, msg);
}

void ga_app_set_connection(ga_app *a, int input, int output)
{
    if (a->input_ready && !input) {
        a->calibrating = 0;
        ga_state_reset(&a->st);
        memset(&a->out, 0, sizeof(a->out));
        message(a, "Controle desconectado. Calibração cancelada.");
    }
    a->input_ready = !!input;
    a->output_ready = !!output;
}

int ga_app_commit(ga_app *a, const ga_config *cfg)
{
    ga_config next = *cfg;
    ga_config_clamp(&next);
    if (ga_profile_save(a->profile_dir, a->title, &next)) {
        a->save_error = 1;
        message(a, "Não foi possível salvar. O ajuste anterior foi mantido.");
        return -2;
    }
    a->undo_cfg = a->cfg;
    a->can_undo = 1;
    a->cfg = next;
    a->title_has_profile = 1;
    a->save_error = 0;
    a->revision++;
    /* Não interprete botões que já estavam segurados como novos cliques. */
    uint32_t held = a->st.prev_buttons;
    ga_state_reset(&a->st);
    a->st.prev_buttons = held;
    return 0;
}

int ga_app_set(ga_app *a, const char *key, const char *value)
{
    ga_config next = a->cfg;
    if (ga_config_set(&next, key, value)) return -1;
    return ga_app_commit(a, &next);
}

int ga_app_save_as_default(ga_app *a)
{
    if (ga_profile_save(a->profile_dir, GA_DEFAULT_PROFILE, &a->cfg)) {
        message(a, "Falha ao salvar o perfil padrão.");
        return -2;
    }
    message(a, "Configurações salvas como padrão para jogos novos.");
    return 0;
}

int ga_app_action(ga_app *a, const char *action)
{
    ga_config next = a->cfg;
    if (!strcmp(action, "pause")) {
        a->paused = !a->paused;
        ga_state_reset(&a->st);
        a->st.prev_buttons = a->last_in.buttons;
        a->revision++;
        message(a, a->paused ? "Gyro pausado; analógico físico disponível." : "Gyro retomado.");
        return 0;
    }
    if (!strcmp(action, "begin_test")) {
        if (a->testing) return -1;
        a->test_cfg = a->cfg;
        a->testing = 1;
        a->revision++;
        message(a, "Teste iniciado. Pode ajustar e voltar ao ponto inicial.");
        return 0;
    }
    if (!strcmp(action, "keep_test")) {
        if (!a->testing) return -1;
        a->testing = 0;
        a->revision++;
        message(a, "Teste concluído. Ajustes mantidos neste jogo.");
        return 0;
    }
    if (!strcmp(action, "revert_test")) {
        if (!a->testing) return -1;
        next = a->test_cfg;
    } else if (!strcmp(action, "undo")) {
        if (!a->can_undo) return -1;
        next = a->undo_cfg;
    } else if (!strcmp(action, "use_default")) {
        ga_profile_load(a->profile_dir, GA_DEFAULT_PROFILE, &next);
    } else if (!strcmp(action, "restore_defaults")) {
        ga_config_default(&next);
    } else if (!strcmp(action, "preset_precision")) {
        next.sensitivity = 1; next.anti_deadzone = .1f;
        next.smoothing = .2f; next.vertical_ratio = .8f;
    } else if (!strcmp(action, "preset_balanced")) {
        next.sensitivity = 1.5f; next.anti_deadzone = .2f;
        next.smoothing = .25f; next.vertical_ratio = .8f;
    } else if (!strcmp(action, "preset_fast")) {
        next.sensitivity = 2.5f; next.anti_deadzone = .2f;
        next.smoothing = .15f; next.vertical_ratio = 1;
    } else if (!strcmp(action, "calibrate")) {
        if (!a->input_ready || a->calibrating) return -1;
        a->calibrating = 1;
        a->calibration_time = a->mean_pitch = a->mean_yaw = 0;
        a->m2_pitch = a->m2_yaw = 0;
        a->calibration_samples = 0;
        a->revision++;
        message(a, "Apoie o controle parado por 2 segundos. Gyro temporariamente suspenso.");
        return 0;
    } else if (!strcmp(action, "cancel_calibration")) {
        a->calibrating = 0;
        a->revision++;
        message(a, "Calibração cancelada.");
        return 0;
    } else return -1;
    int r = ga_app_commit(a, &next);
    if (!r) {
        if (!strcmp(action, "revert_test")) a->testing = 0;
        if (!strcmp(action, "restore_defaults")) {
            a->calibrating = 0;
            message(a, "Padrões de fábrica restaurados neste perfil. Você pode desfazer o ajuste.");
        } else message(a, "Perfil atualizado e salvo. Teste a mira no jogo.");
    }
    return r;
}

static void calibrate_frame(ga_app *a, const ga_input *in)
{
    if (!isfinite(in->pitch_dps) || !isfinite(in->yaw_dps) ||
        !isfinite(in->dt) || in->dt <= 0 || in->dt > .1f ||
        fabsf(in->pitch_dps) > 20 || fabsf(in->yaw_dps) > 20) {
        a->calibrating = 0;
        message(a, "Calibração recusada: movimento ou amostras inválidas. Apoie o controle e repita.");
        return;
    }
    int n = ++a->calibration_samples;
    float dp = in->pitch_dps - a->mean_pitch, dy = in->yaw_dps - a->mean_yaw;
    a->mean_pitch += dp / n; a->mean_yaw += dy / n;
    a->m2_pitch += dp * (in->pitch_dps - a->mean_pitch);
    a->m2_yaw += dy * (in->yaw_dps - a->mean_yaw);
    a->calibration_time += in->dt;
    if (a->calibration_time < 2) return;
    a->calibrating = 0;
    if (n < 50 || a->m2_pitch / n > .25f || a->m2_yaw / n > .25f) {
        message(a, "Controle se moveu durante a calibração. Nada foi alterado; tente novamente.");
        return;
    }
    ga_config next = a->cfg;
    next.bias_pitch = a->mean_pitch; next.bias_yaw = a->mean_yaw;
    if (!ga_app_commit(a, &next)) message(a, "Sensor calibrado e salvo neste perfil.");
}

int ga_app_process(ga_app *a, const ga_input *in)
{
    a->last_in = *in;
    int was_calibrating = a->calibrating;
    if (a->calibrating) calibrate_frame(a, in);
    ga_config frame_cfg = a->cfg;
    if (was_calibrating) frame_cfg.hotkeys_enabled = 0;
    int evt = ga_update(&frame_cfg, &a->st, in, &a->out);
    if (a->paused || was_calibrating ||
        (a->remote_play && !strcmp(a->title, GA_DEFAULT_PROFILE))) {
        ga_config passive = frame_cfg;
        passive.enabled = passive.hotkeys_enabled = 0;
        ga_state state = {0};
        ga_output out;
        ga_update(&passive, &state, in, &out);
        out.suppress = a->out.suppress;
        a->out = out;
        a->st.sx = a->st.sy = 0;
        a->st.toggled = 0;
    }
    if (evt & (GA_EVT_SENS | GA_EVT_ADZ | GA_EVT_ENABLED)) {
        if (!ga_app_commit(a, &frame_cfg)) {
            char msg[256];
            snprintf(msg, sizeof(msg), "%s · gyro %s · sens %.1f · zona %.0f%%", a->title,
                     a->cfg.enabled ? "ligado" : "desligado", a->cfg.sensitivity,
                     a->cfg.anti_deadzone * 100);
            message(a, msg);
        } else {
            /* A saída deste quadro também deve respeitar a configuração mantida. */
            ga_config previous = a->cfg;
            previous.hotkeys_enabled = 0;
            if (a->paused || was_calibrating ||
                (a->remote_play && !strcmp(a->title, GA_DEFAULT_PROFILE))) previous.enabled = 0;
            uint32_t suppress = a->out.suppress;
            ga_update(&previous, &a->st, in, &a->out);
            a->out.suppress = suppress;
        }
    }
    if (evt & GA_EVT_HELP)
        message(a, "Touchpad + ↑/↓: sensibilidade; ←/→: zona morta; △: liga/desliga. Painel no celular: guia, pausa e desfazer.");
    return evt;
}

static void json_str(char *dst, size_t n, const char *s)
{
    size_t j = 0;
    for (; *s && j + 2 < n; s++) {
        if (*s == '"' || *s == '\\') dst[j++] = '\\';
        dst[j++] = (unsigned char)*s < 0x20 ? ' ' : *s;
    }
    dst[j] = 0;
}

size_t ga_app_state_json(const ga_app *a, char *buf, size_t n)
{
    if (!n) return 0;
    char msg[512], title[128], platform[64], name[256], source[64];
    json_str(name, sizeof(name), a->title_name);
    json_str(source, sizeof(source), a->title_source);
    json_str(msg, sizeof(msg), a->message);
    json_str(title, sizeof(title), a->title);
    json_str(platform, sizeof(platform), a->platform);
    size_t len = (size_t)snprintf(buf, n,
        "{\"platform\":\"%s\",\"simulated\":%d,\"remote_play\":%d,\"title\":\"%s\",\"has_profile\":%d,"
        "\"title_name\":\"%s\",\"title_source\":\"%s\",\"message\":\"%s\",\"message_seq\":%u,\"revision\":%u,"
        "\"input_ready\":%d,\"output_ready\":%d,\"paused\":%d,\"can_undo\":%d,\"testing\":%d,\"save_error\":%d,"
        "\"calibration\":{\"active\":%d,\"progress\":%.2f},"
        "\"config\":{\"enabled\":%d,\"activation\":\"%s\",\"toggle_mode\":%d,"
        "\"sensitivity\":%.2f,\"anti_deadzone\":%.2f,\"vertical_ratio\":%.2f,"
        "\"smoothing\":%.2f,\"gyro_deadzone\":%.2f,\"invert_x\":%d,\"invert_y\":%d,"
        "\"hotkeys_enabled\":%d,\"bias_pitch\":%.4f,\"bias_yaw\":%.4f},"
        "\"live\":{\"active\":%d,\"rx\":%.3f,\"ry\":%.3f,\"pitch\":%.1f,\"yaw\":%.1f,\"buttons\":%u},\"buttons\":[",
        platform, a->simulated, a->remote_play, title, a->title_has_profile, name, source, msg, a->message_seq, a->revision,
        a->input_ready, a->output_ready, a->paused, a->can_undo, a->testing, a->save_error,
        a->calibrating, fminf(a->calibration_time / 2, 1),
        a->cfg.enabled, ga_button_id(a->cfg.activation), a->cfg.toggle_mode,
        a->cfg.sensitivity, a->cfg.anti_deadzone, a->cfg.vertical_ratio, a->cfg.smoothing,
        a->cfg.gyro_deadzone, a->cfg.invert_x, a->cfg.invert_y, a->cfg.hotkeys_enabled,
        a->cfg.bias_pitch, a->cfg.bias_yaw, a->out.gyro_active, a->out.rx, a->out.ry,
        isfinite(a->last_in.pitch_dps) ? a->last_in.pitch_dps : 0,
        isfinite(a->last_in.yaw_dps) ? a->last_in.yaw_dps : 0, a->last_in.buttons);
    for (int b = 1; b < GA_BTN_COUNT && len < n; b++)
        len += (size_t)snprintf(buf + len, n - len, "%s{\"id\":\"%s\",\"label\":\"%s\"}",
                               b > 1 ? "," : "", ga_button_id((ga_button)b), ga_button_label((ga_button)b));
    if (len < n) len += (size_t)snprintf(buf + len, n - len, "]}");
    return len < n ? len : n - 1;
}

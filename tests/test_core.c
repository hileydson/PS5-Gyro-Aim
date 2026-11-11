/* Testes do núcleo: rodam no PC, sem controle conectado. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../src/core/app.h"
#include "../src/core/profile.h"

static int fails = 0;
#define CHECK(cond) do { if (!(cond)) { printf("FALHOU %s:%d  %s\n", __FILE__, __LINE__, #cond); fails++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 1e-3f)

static ga_input frame(float pitch, float yaw, uint32_t buttons)
{
    ga_input in = {0};
    in.pitch_dps = pitch;
    in.yaw_dps = yaw;
    in.dt = 1.0f / 250.0f;
    in.buttons = buttons;
    return in;
}

static void test_hold_activation(void)
{
    ga_config cfg; ga_state st; ga_output out;
    ga_config_default(&cfg);
    cfg.smoothing = 0; cfg.anti_deadzone = 0; cfg.gyro_deadzone = 0; cfg.sensitivity = 1;
    ga_state_reset(&st);

    ga_input in = frame(0, 50, 0);
    ga_update(&cfg, &st, &in, &out);
    CHECK(!out.gyro_active && NEAR(out.rx, 0));

    in = frame(0, 50, GA_BIT(GA_BTN_L2));  /* girar para a esquerda */
    ga_update(&cfg, &st, &in, &out);
    CHECK(out.gyro_active);
    CHECK(NEAR(out.rx, -0.5f));             /* 50 °/s * 1.0 / 100 */

    in = frame(40, 0, GA_BIT(GA_BTN_L2));  /* inclinar para cima */
    ga_update(&cfg, &st, &in, &out);
    CHECK(NEAR(out.ry, 0.4f * cfg.vertical_ratio));
}

static void test_toggle(void)
{
    ga_config cfg; ga_state st; ga_output out;
    ga_config_default(&cfg);
    cfg.toggle_mode = 1;
    cfg.activation = GA_BTN_R3;
    ga_state_reset(&st);

    ga_input in = frame(0, 0, GA_BIT(GA_BTN_R3));
    ga_update(&cfg, &st, &in, &out); CHECK(out.gyro_active);
    in.buttons = 0;
    ga_update(&cfg, &st, &in, &out); CHECK(out.gyro_active);   /* continua ligado */
    in.buttons = GA_BIT(GA_BTN_R3);
    ga_update(&cfg, &st, &in, &out); CHECK(!out.gyro_active);  /* segundo toque desliga */
}

static void test_anti_deadzone(void)
{
    ga_config cfg; ga_state st; ga_output out;
    ga_config_default(&cfg);
    cfg.smoothing = 0; cfg.gyro_deadzone = 0; cfg.sensitivity = 1; cfg.anti_deadzone = 0.3f;
    cfg.activation = GA_BTN_ALWAYS;
    ga_state_reset(&st);

    ga_input in = frame(0, -2, 0);         /* movimento pequeno: 0.02 */
    ga_update(&cfg, &st, &in, &out);
    CHECK(out.rx > 0.3f && out.rx < 0.32f); /* pulou para depois da zona morta */

    in = frame(0, -500, 0);                 /* muito rápido: satura em 1 */
    ga_update(&cfg, &st, &in, &out);
    CHECK(NEAR(out.rx, 1.0f));

    in = frame(0, 0, 0);                    /* parado: volta a zero */
    ga_update(&cfg, &st, &in, &out);
    CHECK(NEAR(out.rx, 0) && NEAR(out.ry, 0));
}

static void test_gyro_deadzone(void)
{
    ga_config cfg; ga_state st; ga_output out;
    ga_config_default(&cfg);
    cfg.smoothing = 0; cfg.anti_deadzone = 0.25f; cfg.gyro_deadzone = 1.0f;
    cfg.activation = GA_BTN_ALWAYS;
    ga_state_reset(&st);

    ga_input in = frame(0.5f, -0.6f, 0);    /* tremor da mão */
    ga_update(&cfg, &st, &in, &out);
    CHECK(NEAR(out.rx, 0) && NEAR(out.ry, 0)); /* anti-deadzone não amplifica ruído */
}

static void test_hotkeys(void)
{
    ga_config cfg; ga_state st; ga_output out;
    ga_config_default(&cfg);
    ga_state_reset(&st);
    float s0 = cfg.sensitivity;

    uint32_t mod = GA_BIT(GA_HOTKEY_MODIFIER);
    ga_input in = frame(0, 0, mod);
    ga_update(&cfg, &st, &in, &out);
    in.buttons = mod | GA_BIT(GA_BTN_DPAD_UP);
    int evt = ga_update(&cfg, &st, &in, &out);
    CHECK(evt & GA_EVT_SENS);
    CHECK(NEAR(cfg.sensitivity, s0 + 0.1f));
    CHECK(out.suppress & GA_BIT(GA_BTN_DPAD_UP));

    /* segurar não repete */
    evt = ga_update(&cfg, &st, &in, &out);
    CHECK(evt == 0);

    in.buttons = mod | GA_BIT(GA_BTN_TRIANGLE);
    evt = ga_update(&cfg, &st, &in, &out);
    CHECK((evt & GA_EVT_ENABLED) && !cfg.enabled);

    /* D-pad sem o modificador não mexe na config */
    in.buttons = GA_BIT(GA_BTN_DPAD_UP);
    evt = ga_update(&cfg, &st, &in, &out);
    CHECK(evt == 0 && out.suppress == 0);
}

static void test_profiles_per_game(void)
{
    char dir[] = "/tmp/gyroaim_test_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);

    ga_app app;
    ga_app_init(&app, dir, "test", 1);

    ga_app_set_title(&app, "PPSA00001");
    CHECK(!app.title_has_profile);
    CHECK(ga_app_set(&app, "sensitivity", "3.3") == 0);
    CHECK(ga_app_set(&app, "activation", "R1") == 0);
    CHECK(app.title_has_profile);

    ga_app_set_title(&app, "PPSA00002");      /* outro jogo: volta ao padrão */
    CHECK(!NEAR(app.cfg.sensitivity, 3.3f));
    CHECK(app.cfg.activation == GA_BTN_L2);
    CHECK(ga_app_set(&app, "anti_deadzone", "0.4") == 0);

    ga_app_set_title(&app, "PPSA00001");      /* volta ao primeiro: recupera o perfil */
    CHECK(app.title_has_profile);
    CHECK(NEAR(app.cfg.sensitivity, 3.3f));
    CHECK(app.cfg.activation == GA_BTN_R1);

    ga_app_set_title(&app, "PPSA00002");
    CHECK(NEAR(app.cfg.anti_deadzone, 0.4f));

    /* "usar como padrão" vale para jogos novos */
    CHECK(ga_app_save_as_default(&app) == 0);
    ga_app_set_title(&app, "PPSA00099");
    CHECK(NEAR(app.cfg.anti_deadzone, 0.4f) && !app.title_has_profile);

    /* valores fora da faixa e chaves inválidas */
    CHECK(ga_app_set(&app, "sensitivity", "999") == 0 && NEAR(app.cfg.sensitivity, 10.0f));
    CHECK(ga_app_set(&app, "nao_existe", "1") != 0);
    CHECK(ga_app_set(&app, "activation", "XYZ") != 0);

    /* title id com caracteres perigosos não sai da pasta */
    char safe[64];
    ga_title_sanitize("../../etc/passwd", safe, sizeof(safe));
    CHECK(strcmp(safe, "etcpasswd") == 0);

    char json[8192];
    size_t n = ga_app_state_json(&app, json, sizeof(json));
    CHECK(n > 0 && json[0] == '{' && json[n - 1] == '}');

    char cmd[128];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir);
    CHECK(system(cmd) == 0);
}


static void cleanup(const char *dir)
{
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rm -rf %s", dir); /* only mkdtemp-owned test dirs */
    CHECK(system(cmd) == 0);
}

static void test_validation_and_legacy(void)
{
    ga_config cfg;
    ga_config_default(&cfg);
    float old = cfg.sensitivity;
    const char *bad[] = {"nan", "inf", "-inf", "2garbage", "", "2\n"};
    for (size_t i = 0; i < sizeof(bad)/sizeof(bad[0]); i++) {
        CHECK(ga_config_set(&cfg, "sensitivity", bad[i]) != 0);
        CHECK(NEAR(cfg.sensitivity, old));
    }
    char dir[] = "/tmp/gyroaim_legacy_XXXXXX", path[256];
    CHECK(mkdtemp(dir) != NULL);
    snprintf(path, sizeof(path), "%s/LEGACY.ini", dir);
    FILE *f = fopen(path, "w"); CHECK(f != NULL);
    if (f) { fputs("sensitivity=2.70\nenabled=1\n", f); fclose(f); }
    CHECK(ga_profile_load(dir, "LEGACY", &cfg) == 1);
    CHECK(NEAR(cfg.sensitivity, 2.7f) && cfg.hotkeys_enabled == 1);
    CHECK(NEAR(cfg.bias_pitch, 0) && NEAR(cfg.bias_yaw, 0));
    cfg.bias_pitch = .1234f; cfg.bias_yaw = -.2345f; cfg.hotkeys_enabled = 0;
    CHECK(ga_profile_save(dir, "LEGACY", &cfg) == 0);
    ga_profile_load(dir, "LEGACY", &cfg);
    CHECK(NEAR(cfg.bias_pitch, .1234f) && NEAR(cfg.bias_yaw, -.2345f) && !cfg.hotkeys_enabled);
    cleanup(dir);
}

static void test_testpoint_and_failures(void)
{
    char dir[] = "/tmp/gyroaim_actions_XXXXXX";
    CHECK(mkdtemp(dir) != NULL);
    ga_app a;
    ga_app_init(&a, dir, "test", 1);
    ga_app_set_title(&a, "GAME_A");
    CHECK(ga_app_set(&a, "sensitivity", "2.2") == 0);
    CHECK(ga_app_action(&a, "begin_test") == 0 && a.testing);
    CHECK(ga_app_action(&a, "begin_test") != 0); /* do not overwrite checkpoint */
    CHECK(ga_app_action(&a, "preset_fast") == 0 && NEAR(a.cfg.sensitivity, 2.5f));
    CHECK(ga_app_set(&a, "sensitivity", "4.2") == 0);
    CHECK(ga_app_action(&a, "revert_test") == 0 && !a.testing);
    CHECK(NEAR(a.cfg.sensitivity, 2.2f));
    ga_config reloaded;
    CHECK(ga_profile_load(dir, "GAME_A", &reloaded) == 1 && NEAR(reloaded.sensitivity, 2.2f));
    CHECK(ga_app_action(&a, "undo") == 0 && NEAR(a.cfg.sensitivity, 4.2f));
    ga_app_action(&a, "begin_test");
    ga_app_set_title(&a, "GAME_B");
    CHECK(!a.testing && !a.can_undo && ga_app_action(&a, "revert_test") != 0);
    CHECK(ga_app_set(&a, "sensitivity", "3.2") == 0);
    CHECK(ga_app_save_as_default(&a) == 0);
    ga_app_set_title(&a, "GAME_A");
    CHECK(ga_app_action(&a, "use_default") == 0 && NEAR(a.cfg.sensitivity, 3.2f));

    /* A file in place of the profile directory reliably fails even when run as root. */
    char file[256]; snprintf(file, sizeof(file), "%s/not_a_directory", dir);
    FILE *f = fopen(file, "w"); CHECK(f != NULL); if (f) fclose(f);
    snprintf(a.profile_dir, sizeof(a.profile_dir), "%s/not_a_directory/subdir", dir);
    float before = a.cfg.sensitivity; unsigned rev = a.revision;
    CHECK(ga_app_set(&a, "sensitivity", "7") == -2);
    CHECK(NEAR(a.cfg.sensitivity, before) && a.save_error && a.revision == rev);
    CHECK(ga_app_save_as_default(&a) == -2);
    a.cfg.enabled = 0; a.cfg.activation = GA_BTN_ALWAYS;
    ga_input input = frame(0, 100, GA_BIT(GA_HOTKEY_MODIFIER) | GA_BIT(GA_BTN_TRIANGLE));
    ga_app_process(&a, &input);
    CHECK(!a.cfg.enabled && !a.out.gyro_active && NEAR(a.out.rx, 0));
    CHECK(a.out.suppress & GA_BIT(GA_BTN_TRIANGLE));
    cleanup(dir);
}

static void test_pause_help_and_passthrough(void)
{
    char dir[] = "/tmp/gyroaim_pause_XXXXXX"; CHECK(mkdtemp(dir) != NULL);
    ga_app a; ga_app_init(&a, dir, "test", 1);
    ga_app_set_title(&a, "GAME");
    CHECK(ga_app_set(&a, "activation", "ALWAYS") == 0);
    ga_input in = frame(10, 50, 0); in.real_rx = .3f; in.real_ry = .2f;
    ga_app_process(&a, &in); CHECK(a.out.gyro_active);
    CHECK(ga_app_action(&a, "pause") == 0);
    ga_app_process(&a, &in);
    CHECK(!a.out.gyro_active && NEAR(a.out.rx, .3f) && NEAR(a.out.ry, .2f));
    CHECK(a.cfg.enabled); /* pause is never persisted */
    in.buttons = GA_BIT(GA_HOTKEY_MODIFIER) | GA_BIT(GA_BTN_DPAD_UP);
    ga_app_process(&a, &in); CHECK(a.cfg.enabled && a.paused);
    in.buttons = GA_BIT(GA_HOTKEY_MODIFIER) | GA_BIT(GA_BTN_TRIANGLE);
    ga_app_process(&a, &in); CHECK(!a.cfg.enabled && !a.out.gyro_active);
    unsigned revision = a.revision;
    in.buttons = GA_BIT(GA_HOTKEY_MODIFIER) | GA_BIT(GA_BTN_SQUARE);
    CHECK(ga_app_process(&a, &in) & GA_EVT_HELP);
    CHECK(a.revision == revision && (a.out.suppress & GA_BIT(GA_BTN_SQUARE)));
    CHECK(ga_app_set(&a, "hotkeys_enabled", "0") == 0);
    in.buttons = GA_BIT(GA_HOTKEY_MODIFIER) | GA_BIT(GA_BTN_DPAD_DOWN);
    CHECK(ga_app_process(&a, &in) == 0 && a.out.suppress == 0);

    a.remote_play = 1;
    ga_app_set_title(&a, "default"); a.cfg.activation = GA_BTN_ALWAYS;
    ga_app_process(&a, &in);
    CHECK(!a.out.gyro_active && NEAR(a.out.rx, .3f)); /* no gyro in unknown Remote Play title */
    cleanup(dir);
}

static void test_calibration(void)
{
    char dir[] = "/tmp/gyroaim_calibrate_XXXXXX"; CHECK(mkdtemp(dir) != NULL);
    ga_app a; ga_app_init(&a, dir, "test", 1);
    CHECK(ga_app_action(&a, "calibrate") != 0);
    ga_app_set_connection(&a, 1, 1);
    CHECK(ga_app_action(&a, "calibrate") == 0);
    ga_input in = frame(.4f, -.2f, 0);
    in.real_rx = .25f;
    for (int i = 0; i < 510; i++) {
        ga_app_process(&a, &in);
        CHECK(!a.out.gyro_active && NEAR(a.out.rx, .25f));
    }
    CHECK(!a.calibrating && NEAR(a.cfg.bias_pitch, .4f) && NEAR(a.cfg.bias_yaw, -.2f));
    ga_config loaded; ga_profile_load(dir, "default", &loaded);
    CHECK(NEAR(loaded.bias_pitch, .4f));
    CHECK(ga_app_action(&a, "calibrate") == 0);
    for (int i = 0; i < 510; i++) {
        in.pitch_dps = (i % 2) ? 2 : -2;
        ga_app_process(&a, &in);
    }
    CHECK(!a.calibrating && NEAR(a.cfg.bias_pitch, .4f));
    CHECK(ga_app_action(&a, "calibrate") == 0);
    ga_app_set_connection(&a, 0, 0);
    CHECK(!a.calibrating);
    ga_app_set_connection(&a, 1, 1);
    ga_app_action(&a, "calibrate");
    ga_app_set_title(&a, "OTHER_GAME");
    CHECK(!a.calibrating && NEAR(a.cfg.bias_pitch, .4f));
    ga_config cfg; ga_config_default(&cfg); ga_state st; ga_output out;
    cfg.activation = GA_BTN_ALWAYS; cfg.bias_pitch = .4f; cfg.bias_yaw = -.2f;
    cfg.smoothing = 0; cfg.gyro_deadzone = 0;
    ga_state_reset(&st); in = frame(.4f, -.2f, 0);
    ga_update(&cfg, &st, &in, &out);
    CHECK(NEAR(out.rx, 0) && NEAR(out.ry, 0));
    in.pitch_dps = NAN; in.yaw_dps = INFINITY; in.real_rx = NAN;
    ga_update(&cfg, &st, &in, &out);
    CHECK(isfinite(out.rx) && isfinite(out.ry));
    CHECK(ga_app_state_json(&a, NULL, 0) == 0);
    cleanup(dir);
}

int main(void)
{
    test_hold_activation();
    test_toggle();
    test_anti_deadzone();
    test_gyro_deadzone();
    test_hotkeys();
    test_profiles_per_game();
    test_validation_and_legacy();
    test_testpoint_and_failures();
    test_pause_help_and_passthrough();
    test_calibration();
    if (fails == 0) printf("todos os testes passaram\n");
    return fails ? 1 : 0;
}

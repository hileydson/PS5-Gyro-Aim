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

int main(void)
{
    test_hold_activation();
    test_toggle();
    test_anti_deadzone();
    test_gyro_deadzone();
    test_hotkeys();
    test_profiles_per_game();
    if (fails == 0) printf("todos os testes passaram\n");
    return fails ? 1 : 0;
}

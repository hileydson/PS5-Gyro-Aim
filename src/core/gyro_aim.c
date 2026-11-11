#include "gyro_aim.h"

#include <math.h>
#include <string.h>

static const struct { const char *id, *label; } BUTTONS[GA_BTN_COUNT] = {
    [GA_BTN_NONE]       = {"NONE", "Nenhum"},
    [GA_BTN_L1]         = {"L1", "L1"},
    [GA_BTN_R1]         = {"R1", "R1"},
    [GA_BTN_L2]         = {"L2", "L2"},
    [GA_BTN_R2]         = {"R2", "R2"},
    [GA_BTN_L3]         = {"L3", "L3 (clique analógico esq.)"},
    [GA_BTN_R3]         = {"R3", "R3 (clique analógico dir.)"},
    [GA_BTN_CROSS]      = {"CROSS", "X"},
    [GA_BTN_CIRCLE]     = {"CIRCLE", "Círculo"},
    [GA_BTN_SQUARE]     = {"SQUARE", "Quadrado"},
    [GA_BTN_TRIANGLE]   = {"TRIANGLE", "Triângulo"},
    [GA_BTN_DPAD_UP]    = {"UP", "D-pad cima"},
    [GA_BTN_DPAD_DOWN]  = {"DOWN", "D-pad baixo"},
    [GA_BTN_DPAD_LEFT]  = {"LEFT", "D-pad esquerda"},
    [GA_BTN_DPAD_RIGHT] = {"RIGHT", "D-pad direita"},
    [GA_BTN_TOUCHPAD]   = {"TOUCHPAD", "Touchpad (clique)"},
    [GA_BTN_OPTIONS]    = {"OPTIONS", "Options"},
    [GA_BTN_CREATE]     = {"CREATE", "Create"},
    [GA_BTN_ALWAYS]     = {"ALWAYS", "Sempre ligado"},
};

const char *ga_button_id(ga_button b)
{
    return (b >= 0 && b < GA_BTN_COUNT) ? BUTTONS[b].id : "NONE";
}

const char *ga_button_label(ga_button b)
{
    return (b >= 0 && b < GA_BTN_COUNT) ? BUTTONS[b].label : "?";
}

ga_button ga_button_from_id(const char *id)
{
    for (int i = 0; i < GA_BTN_COUNT; i++)
        if (strcmp(BUTTONS[i].id, id) == 0)
            return (ga_button)i;
    return GA_BTN_NONE;
}

void ga_config_default(ga_config *cfg)
{
    cfg->enabled        = 1;
    cfg->activation     = GA_BTN_L2; /* mirar = segurar L2, como na maioria dos jogos */
    cfg->toggle_mode    = 0;
    cfg->sensitivity    = 1.5f;
    cfg->anti_deadzone  = 0.20f;
    cfg->vertical_ratio = 0.8f;
    cfg->smoothing      = 0.25f;
    cfg->gyro_deadzone  = 1.0f;
    cfg->invert_x       = 0;
    cfg->invert_y       = 0;
}

static float clampf(float v, float lo, float hi)
{
    if (!(v >= lo)) return lo; /* também trata NaN */
    return v > hi ? hi : v;
}

void ga_config_clamp(ga_config *cfg)
{
    if (cfg->activation <= GA_BTN_NONE || cfg->activation >= GA_BTN_COUNT)
        cfg->activation = GA_BTN_L2;
    cfg->enabled        = cfg->enabled ? 1 : 0;
    cfg->toggle_mode    = cfg->toggle_mode ? 1 : 0;
    cfg->invert_x       = cfg->invert_x ? 1 : 0;
    cfg->invert_y       = cfg->invert_y ? 1 : 0;
    cfg->sensitivity    = clampf(cfg->sensitivity, 0.1f, 10.0f);
    cfg->anti_deadzone  = clampf(cfg->anti_deadzone, 0.0f, 0.9f);
    cfg->vertical_ratio = clampf(cfg->vertical_ratio, 0.1f, 2.0f);
    cfg->smoothing      = clampf(cfg->smoothing, 0.0f, 0.9f);
    cfg->gyro_deadzone  = clampf(cfg->gyro_deadzone, 0.0f, 10.0f);
}

void ga_state_reset(ga_state *st)
{
    memset(st, 0, sizeof(*st));
}

/* Atalhos: segurando o touchpad (clique), o D-pad ajusta a configuração. */
static int handle_hotkeys(ga_config *cfg, uint32_t pressed, uint32_t held, ga_output *out)
{
    int evt = 0;
    if (!(held & GA_BIT(GA_HOTKEY_MODIFIER)))
        return 0;

    out->suppress |= GA_BIT(GA_HOTKEY_MODIFIER) | GA_BIT(GA_BTN_DPAD_UP) |
                     GA_BIT(GA_BTN_DPAD_DOWN) | GA_BIT(GA_BTN_DPAD_LEFT) |
                     GA_BIT(GA_BTN_DPAD_RIGHT) | GA_BIT(GA_BTN_TRIANGLE);

    if (pressed & GA_BIT(GA_BTN_DPAD_UP))    { cfg->sensitivity += 0.1f;   evt |= GA_EVT_SENS; }
    if (pressed & GA_BIT(GA_BTN_DPAD_DOWN))  { cfg->sensitivity -= 0.1f;   evt |= GA_EVT_SENS; }
    if (pressed & GA_BIT(GA_BTN_DPAD_RIGHT)) { cfg->anti_deadzone += 0.02f; evt |= GA_EVT_ADZ; }
    if (pressed & GA_BIT(GA_BTN_DPAD_LEFT))  { cfg->anti_deadzone -= 0.02f; evt |= GA_EVT_ADZ; }
    if (pressed & GA_BIT(GA_BTN_TRIANGLE))   { cfg->enabled = !cfg->enabled; evt |= GA_EVT_ENABLED; }

    if (evt) {
        /* arredonda para não acumular erro de float (1.5000001) */
        cfg->sensitivity   = roundf(cfg->sensitivity * 10.0f) / 10.0f;
        cfg->anti_deadzone = roundf(cfg->anti_deadzone * 100.0f) / 100.0f;
        ga_config_clamp(cfg);
    }
    return evt;
}

int ga_update(ga_config *cfg, ga_state *st, const ga_input *in, ga_output *out)
{
    uint32_t held    = in->buttons;
    uint32_t pressed = held & ~st->prev_buttons;
    st->prev_buttons = held;

    out->suppress = 0;
    int evt = handle_hotkeys(cfg, pressed, held, out);

    /* Ativação */
    int active;
    if (!cfg->enabled) {
        active = 0;
        st->toggled = 0;
    } else if (cfg->activation == GA_BTN_ALWAYS) {
        active = 1;
    } else if (cfg->toggle_mode) {
        if (pressed & GA_BIT(cfg->activation))
            st->toggled = !st->toggled;
        active = st->toggled;
    } else {
        active = (held & GA_BIT(cfg->activation)) != 0;
    }

    float gx = 0.0f, gy = 0.0f;
    if (active) {
        float yaw = in->yaw_dps, pitch = in->pitch_dps;

        /* deadzone do gyro (subtrativa, sem "degrau") */
        float mag = sqrtf(yaw * yaw + pitch * pitch);
        if (mag <= cfg->gyro_deadzone) {
            yaw = pitch = 0.0f;
        } else {
            float k = (mag - cfg->gyro_deadzone) / mag;
            yaw *= k;
            pitch *= k;
        }

        /* 1.0 de sensibilidade: 100 °/s = analógico no máximo */
        float tx = -yaw * cfg->sensitivity / 100.0f;
        float ty = pitch * cfg->sensitivity * cfg->vertical_ratio / 100.0f;
        if (cfg->invert_x) tx = -tx;
        if (cfg->invert_y) ty = -ty;

        /* suavização exponencial, independente da taxa de quadros */
        float a = cfg->smoothing;
        if (a > 0.0f && in->dt > 0.0f)
            a = powf(a, in->dt * 60.0f); /* calibrado para 60 Hz */
        st->sx = st->sx * a + tx * (1.0f - a);
        st->sy = st->sy * a + ty * (1.0f - a);
        gx = st->sx;
        gy = st->sy;

        /* anti-deadzone radial: pula a zona morta interna do jogo */
        float m = sqrtf(gx * gx + gy * gy);
        if (m < 0.002f) {
            gx = gy = 0.0f;
        } else {
            float mc = m > 1.0f ? 1.0f : m;
            float nm = cfg->anti_deadzone + (1.0f - cfg->anti_deadzone) * mc;
            gx = gx / m * nm;
            gy = gy / m * nm;
        }
    } else {
        st->sx = st->sy = 0.0f;
    }

    /* soma com o analógico físico (o jogador ainda pode usar o stick) */
    float rx = in->real_rx + gx;
    float ry = in->real_ry + gy;
    float m = sqrtf(rx * rx + ry * ry);
    if (m > 1.0f) { rx /= m; ry /= m; }

    out->rx = rx;
    out->ry = ry;
    out->gyro_active = active;
    return evt;
}

/*
 * PS5 Gyro Aim - núcleo portátil (Linux e PS5).
 *
 * Converte a velocidade angular do giroscópio em deflexão do analógico direito,
 * com botão de ativação, sensibilidade, anti-deadzone e atalhos no controle.
 * Não depende de nenhuma API de plataforma.
 */
#ifndef GYRO_AIM_H
#define GYRO_AIM_H

#include <stdint.h>

/* Botões do controle (bit = 1u << valor). */
typedef enum {
    GA_BTN_NONE = 0,
    GA_BTN_L1, GA_BTN_R1, GA_BTN_L2, GA_BTN_R2, GA_BTN_L3, GA_BTN_R3,
    GA_BTN_CROSS, GA_BTN_CIRCLE, GA_BTN_SQUARE, GA_BTN_TRIANGLE,
    GA_BTN_DPAD_UP, GA_BTN_DPAD_DOWN, GA_BTN_DPAD_LEFT, GA_BTN_DPAD_RIGHT,
    GA_BTN_TOUCHPAD, GA_BTN_OPTIONS, GA_BTN_CREATE,
    GA_BTN_ALWAYS, /* não é um botão: gyro sempre ligado */
    GA_BTN_COUNT
} ga_button;

#define GA_BIT(b) (1u << (unsigned)(b))

/* Botão que, segurado, transforma o D-pad em atalhos de configuração. */
#define GA_HOTKEY_MODIFIER GA_BTN_TOUCHPAD

typedef struct {
    int       enabled;        /* 0 = gyro desligado neste jogo */
    ga_button activation;     /* botão que liga o gyro */
    int       toggle_mode;    /* 0 = segurar, 1 = apertar liga/desliga */
    float     sensitivity;    /* 0.1 .. 10  (1.0 = 100 °/s vira analógico no máximo) */
    float     anti_deadzone;  /* 0 .. 0.9   (deflexão mínima enviada ao jogo) */
    float     vertical_ratio; /* 0.1 .. 2   (sensibilidade vertical relativa) */
    float     smoothing;      /* 0 .. 0.9   (suavização exponencial) */
    float     gyro_deadzone;  /* 0 .. 10 °/s (ignora tremor/ruído) */
    int       invert_x;
    int       invert_y;
    int       hotkeys_enabled; /* pode desligar se o jogo usa clique do touchpad */
    float     bias_pitch, bias_yaw; /* calibração do sensor em °/s */
} ga_config;

/* Entrada de um quadro. Convenção: pitch > 0 = controle inclinado para cima,
 * yaw > 0 = controle girado para a esquerda. Analógicos em -1..1, y > 0 = cima. */
typedef struct {
    float    pitch_dps;
    float    yaw_dps;
    float    dt;            /* segundos desde o último quadro */
    uint32_t buttons;       /* máscara GA_BIT() */
    float    real_rx, real_ry;
} ga_input;

typedef struct {
    float    rx, ry;        /* analógico direito final (-1..1, y > 0 = cima) */
    int      gyro_active;
    uint32_t suppress;      /* botões que não devem chegar ao jogo (atalhos) */
} ga_output;

typedef struct {
    int      toggled;
    uint32_t prev_buttons;
    float    sx, sy;        /* estado da suavização */
} ga_state;

/* Eventos retornados por ga_update (para notificar e salvar o perfil). */
enum {
    GA_EVT_SENS    = 1 << 0,
    GA_EVT_ADZ     = 1 << 1,
    GA_EVT_ENABLED = 1 << 2,
    GA_EVT_HELP    = 1 << 3,
};

void        ga_config_default(ga_config *cfg);
void        ga_config_clamp(ga_config *cfg);
void        ga_state_reset(ga_state *st);
int         ga_update(ga_config *cfg, ga_state *st, const ga_input *in, ga_output *out);

const char *ga_button_id(ga_button b);   /* "L2", "R1", ... */
const char *ga_button_label(ga_button b);/* nome para a interface */
ga_button   ga_button_from_id(const char *id);

#endif

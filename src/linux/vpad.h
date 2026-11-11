/*
 * Controle virtual (uinput) para testar o Gyro Aim em qualquer jogo/app do PC.
 *
 * Aparece no sistema como um controle de Xbox 360, o layout mais aceito por
 * jogos nativos, Proton e Steam. Recebe os botões e o analógico esquerdo do
 * DualSense, e o analógico direito já vem com o gyro aplicado.
 */
#ifndef GA_VPAD_H
#define GA_VPAD_H

#include <linux/input.h>

typedef struct {
    int fd;
    struct input_absinfo src[ABS_CNT]; /* faixas do DualSense real */
    int last_rx, last_ry;
    int block_hotkeys;
    int face[2], hat[2]; /* físico: restaura ao soltar o modificador */
} vpad;

/* src_fd: o evdev do controle real (para ler as faixas dos eixos). 0 = ok. */
int  vpad_open(vpad *v, int src_fd);
void vpad_close(vpad *v);

/* Repassa um evento do controle real. block_hotkeys = 1 enquanto o modificador
 * de atalhos está segurado (D-pad e triângulo não chegam ao jogo). */
void vpad_forward(vpad *v, const struct input_event *ev, int block_hotkeys);
void vpad_hotkeys(vpad *v, int block_hotkeys);

/* Analógico direito final (-1..1, y > 0 = cima). */
void vpad_right_stick(vpad *v, float rx, float ry);

#endif

/*
 * Simulador Linux do PS5 Gyro Aim.
 *
 * Lê o DualSense (ou DualShock 4) pelo evdev do kernel (driver hid-playstation),
 * roda exatamente o mesmo núcleo do PS5, mostra o analógico virtual no terminal
 * e serve a página de configuração em http://localhost:8090.
 *
 * Teclas: 1-4 = "abre" um jogo simulado, 0 = sem jogo, q = sair.
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

#include "../core/app.h"
#include "../core/webui.h"
#include "vpad.h"

#define MOTION_SUFFIX " Motion Sensors"

typedef struct {
    int   pad, motion, touch;
    char  name[256];
    float gyro_res[3];          /* unidades por °/s */
    struct input_absinfo rs[2]; /* faixa do analógico direito */
} controller;

static volatile sig_atomic_t g_quit = 0;
static char g_vstatus[160] = "desligado";
static struct termios g_old_tio;
static int g_tio_saved = 0;

static void log_notification(const char *msg) { fprintf(stderr, "%s\n", msg); }

static void on_signal(int s) { (void)s; g_quit = 1; }

static void term_restore(void)
{
    if (g_tio_saved) tcsetattr(STDIN_FILENO, TCSANOW, &g_old_tio);
    printf("\x1b[?25h\n");
}

static void term_raw(void)
{
    if (!isatty(STDIN_FILENO)) return;
    struct termios t;
    tcgetattr(STDIN_FILENO, &g_old_tio);
    g_tio_saved = 1;
    t = g_old_tio;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    printf("\x1b[?25l");
}

static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec / 1e9;
}

static void ctl_close(controller *c)
{
    if (c->pad >= 0) close(c->pad);
    if (c->motion >= 0) close(c->motion);
    if (c->touch >= 0) close(c->touch);
    c->pad = c->motion = c->touch = -1;
    c->name[0] = '\0';
}

static int open_by_name(const char *want)
{
    char path[64], name[256];
    for (int i = 0; i < 64; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0 && strcmp(name, want) == 0)
            return fd;
        close(fd);
    }
    return -1;
}

/* Procura qualquer "<nome> Motion Sensors" e abre os três dispositivos do controle. */
static int ctl_find(controller *c)
{
    char path[64], name[256];
    for (int i = 0; i < 64; i++) {
        snprintf(path, sizeof(path), "/dev/input/event%d", i);
        int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        size_t sl = strlen(MOTION_SUFFIX);
        if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0 || strlen(name) <= sl ||
            strcmp(name + strlen(name) - sl, MOTION_SUFFIX) != 0) {
            close(fd);
            continue;
        }
        name[strlen(name) - sl] = '\0';
        char touch_name[300];
        snprintf(touch_name, sizeof(touch_name), "%s Touchpad", name);

        c->motion = fd;
        c->pad = open_by_name(name);
        c->touch = open_by_name(touch_name);
        if (c->pad < 0) { ctl_close(c); continue; }
        snprintf(c->name, sizeof(c->name), "%s", name);

        for (int a = 0; a < 3; a++) {
            struct input_absinfo ai;
            c->gyro_res[a] = 1024.0f; /* valor do hid-playstation */
            if (ioctl(fd, EVIOCGABS(ABS_RX + a), &ai) == 0 && ai.resolution > 0)
                c->gyro_res[a] = (float)ai.resolution;
        }
        for (int a = 0; a < 2; a++) {
            c->rs[a] = (struct input_absinfo){.minimum = 0, .maximum = 255};
            ioctl(c->pad, EVIOCGABS(ABS_RX + a), &c->rs[a]);
        }
        return 0;
    }
    return -1;
}

static float stick_norm(int v, const struct input_absinfo *ai)
{
    float mid = (ai->minimum + ai->maximum) / 2.0f;
    float half = (ai->maximum - ai->minimum) / 2.0f;
    float n = half > 0 ? (v - mid) / half : 0.0f;
    if (n > -0.08f && n < 0.08f) n = 0.0f; /* evita drift do stick físico */
    return n < -1 ? -1 : n > 1 ? 1 : n;
}

static ga_button key_to_button(int code)
{
    switch (code) {
    case BTN_SOUTH:  return GA_BTN_CROSS;
    case BTN_EAST:   return GA_BTN_CIRCLE;
    case BTN_NORTH:  return GA_BTN_TRIANGLE;
    case BTN_WEST:   return GA_BTN_SQUARE;
    case BTN_TL:     return GA_BTN_L1;
    case BTN_TR:     return GA_BTN_R1;
    case BTN_TL2:    return GA_BTN_L2;
    case BTN_TR2:    return GA_BTN_R2;
    case BTN_THUMBL: return GA_BTN_L3;
    case BTN_THUMBR: return GA_BTN_R3;
    case BTN_START:  return GA_BTN_OPTIONS;
    case BTN_SELECT: return GA_BTN_CREATE;
    default:         return GA_BTN_NONE;
    }
}

static void set_bit(uint32_t *m, ga_button b, int on)
{
    if (b == GA_BTN_NONE) return;
    if (on) *m |= GA_BIT(b); else *m &= ~GA_BIT(b);
}

static void draw(const ga_app *app, const controller *c, int port)
{
    const int W = 31, H = 15;
    char grid[15][32];
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            float fx = (x - W / 2) / (float)(W / 2), fy = (y - H / 2) / (float)(H / 2);
            float d = fx * fx + fy * fy;
            grid[y][x] = (d > 0.85f && d < 1.15f) ? '.' : (x == W / 2 || y == H / 2) ? '+' : ' ';
        }
        grid[y][W] = '\0';
    }
    int px = W / 2 + (int)(app->out.rx * (W / 2) + (app->out.rx >= 0 ? 0.5f : -0.5f));
    int py = H / 2 - (int)(app->out.ry * (H / 2) + (app->out.ry >= 0 ? 0.5f : -0.5f));
    grid[py][px] = app->out.gyro_active ? '@' : 'o';

    printf("\x1b[H\x1b[2J");
    printf("PS5 Gyro Aim - simulador Linux    config: http://localhost:%d\n", port);
    printf("Controle: %s\n", c->name[0] ? c->name : "(nenhum - conecte o DualSense por USB/Bluetooth)");
    printf("Controle virtual: %s\n", g_vstatus);
    printf("Jogo: %-12s %s\n\n", app->title, app->title_has_profile ? "[perfil salvo]" : "[usando padrao]");
    for (int y = 0; y < H; y++) {
        printf("   %s   ", grid[y]);
        switch (y) {
        case 1:  printf("Gyro: %s", !app->cfg.enabled ? "DESLIGADO no jogo" :
                         app->out.gyro_active ? "ATIVO" : "inativo"); break;
        case 2:  printf("Ativacao: %s (%s)", ga_button_id(app->cfg.activation),
                         app->cfg.toggle_mode ? "liga/desliga" : "segurar"); break;
        case 4:  printf("Sensibilidade:  %.1f", app->cfg.sensitivity); break;
        case 5:  printf("Anti-deadzone:  %.0f%%", app->cfg.anti_deadzone * 100); break;
        case 7:  printf("Analogico dir.: x=%+.2f y=%+.2f", app->out.rx, app->out.ry); break;
        case 8:  printf("Gyro: pitch %+5.0f  yaw %+5.0f  graus/s",
                         app->last_in.pitch_dps, app->last_in.yaw_dps); break;
        case 10: printf("Msg: %s", app->message); break;
        case 12: printf("Teclas: 1-4 jogo simulado, 0 sem jogo, q sair"); break;
        case 13: printf("Atalhos: segure Touchpad + D-pad / Triangulo"); break;
        }
        printf("\n");
    }
    fflush(stdout);
}

static const char *SIM_TITLES[] = {"default", "PPSA00001", "PPSA00002", "PPSA00003", "PPSA00004"};

int main(int argc, char **argv)
{
    int port = 8090, use_virtual = 1, grab = 1, remote_play = 0, use_controller = 1;
    const char *profile_dir = "profiles", *title = "default";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) port = atoi(argv[++i]);
        else if (strcmp(argv[i], "--profiles") == 0 && i + 1 < argc) profile_dir = argv[++i];
        else if (strcmp(argv[i], "--title") == 0 && i + 1 < argc) title = argv[++i];
        else if (strcmp(argv[i], "--no-virtual") == 0) use_virtual = 0;
        else if (strcmp(argv[i], "--no-grab") == 0) grab = 0;
        else if (strcmp(argv[i], "--remote-play") == 0) remote_play = 1;
        else if (strcmp(argv[i], "--no-controller") == 0) use_controller = 0;
        else {
            fprintf(stderr, "uso: %s [--port 8090] [--profiles ./profiles] [--title JOGO] [--no-virtual] [--no-grab] [--no-controller] [--remote-play]\n", argv[0]);
            return 1;
        }
    }

    static ga_app app;
    ga_app_init(&app, profile_dir, "linux", 1);
    ga_app_set_title(&app, title);
    app.remote_play = remote_play;
    int terminal_output = isatty(STDOUT_FILENO);
    if (!terminal_output) app.notify = log_notification;

    static ga_webui web;
    int werr = ga_webui_start(&web, port);
    if (werr != 0) {
        fprintf(stderr, "nao consegui abrir a porta %d: %s\n", port, strerror(-werr));
        return 1;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    term_raw();
    atexit(term_restore);

    controller c = {.pad = -1, .motion = -1, .touch = -1};
    vpad vp = {.fd = -1};
    snprintf(g_vstatus, sizeof(g_vstatus), "%s", use_virtual ? "aguardando o controle" : "desligado (--no-virtual)");
    ga_input in = {0};
    double last_scan = 0, last_draw = 0, last_frame = now_s(), last_motion = 0;
    float gyro[3] = {0};
    char previous_status[160] = "";
    if (!terminal_output) fprintf(stderr, "Gyro Aim: http://localhost:%d · treino: /practice · perfil %s\n", port, app.title);

    while (!g_quit) {
        double t = now_s();
        if (use_controller && c.motion < 0 && t - last_scan > 2.0) {
            last_scan = t;
            if (ctl_find(&c) == 0 && use_virtual) {
                int verr = vpad_open(&vp, c.pad);
                if (verr != 0) {
                    snprintf(g_vstatus, sizeof(g_vstatus), "ERRO ao criar (%s) - veja o README", strerror(-verr));
                } else if (grab && ioctl(c.pad, EVIOCGRAB, 1) == 0) {
                    snprintf(g_vstatus, sizeof(g_vstatus), "ATIVO (DualSense real escondido dos apps evdev)");
                } else if (remote_play && grab) {
                    vpad_close(&vp);
                    ctl_close(&c);
                    snprintf(g_vstatus, sizeof(g_vstatus), "CONTROLE OCUPADO: encerre a outra sessao do Gyro Aim");
                } else {
                    snprintf(g_vstatus, sizeof(g_vstatus), "ATIVO (DualSense real tambem visivel)");
                }
            }
        }

        struct pollfd fds[5];
        int nfd = 0;
        fds[nfd++] = (struct pollfd){.fd = isatty(STDIN_FILENO) ? STDIN_FILENO : -1, .events = POLLIN};
        fds[nfd++] = (struct pollfd){.fd = web.fd, .events = POLLIN};
        if (c.motion >= 0) fds[nfd++] = (struct pollfd){.fd = c.motion, .events = POLLIN};
        if (c.pad >= 0)    fds[nfd++] = (struct pollfd){.fd = c.pad, .events = POLLIN};
        if (c.touch >= 0)  fds[nfd++] = (struct pollfd){.fd = c.touch, .events = POLLIN};
        poll(fds, (nfds_t)nfd, 20);

        char ch;
        while ((fds[0].revents & POLLIN) && read(STDIN_FILENO, &ch, 1) == 1) {
            if (ch == 'q' || ch == 'Q') g_quit = 1;
            else if (ch >= '0' && ch <= '4') ga_app_set_title(&app, SIM_TITLES[ch - '0']);
        }

        ga_webui_poll(&web, &app);

        struct input_event ev;
        int lost = 0;

        /* Leia o touchpad antes dos outros botões para os atalhos não vazarem. */
        while (c.touch >= 0 && read(c.touch, &ev, sizeof(ev)) == sizeof(ev))
            if (ev.type == EV_KEY && ev.code == BTN_LEFT)
                set_bit(&in.buttons, GA_BTN_TOUCHPAD, ev.value != 0);
        int block = app.cfg.hotkeys_enabled && (in.buttons & GA_BIT(GA_HOTKEY_MODIFIER));
        vpad_hotkeys(&vp, block);

        while (c.pad >= 0 && read(c.pad, &ev, sizeof(ev)) == sizeof(ev)) {
            vpad_forward(&vp, &ev, block);
            if (ev.type == EV_KEY) {
                set_bit(&in.buttons, key_to_button(ev.code), ev.value != 0);
            } else if (ev.type == EV_ABS) {
                if (ev.code == ABS_HAT0X) {
                    set_bit(&in.buttons, GA_BTN_DPAD_LEFT, ev.value < 0);
                    set_bit(&in.buttons, GA_BTN_DPAD_RIGHT, ev.value > 0);
                } else if (ev.code == ABS_HAT0Y) {
                    set_bit(&in.buttons, GA_BTN_DPAD_UP, ev.value < 0);
                    set_bit(&in.buttons, GA_BTN_DPAD_DOWN, ev.value > 0);
                } else if (ev.code == ABS_RX) {
                    in.real_rx = stick_norm(ev.value, &c.rs[0]);
                } else if (ev.code == ABS_RY) {
                    in.real_ry = -stick_norm(ev.value, &c.rs[1]);
                }
            }
        }
        if (c.pad >= 0 && errno == ENODEV) lost = 1;

        int got_frame = 0;
        while (c.motion >= 0 && read(c.motion, &ev, sizeof(ev)) == sizeof(ev)) {
            if (ev.type == EV_ABS && ev.code >= ABS_RX && ev.code <= ABS_RZ)
                gyro[ev.code - ABS_RX] = ev.value / c.gyro_res[ev.code - ABS_RX];
            else if (ev.type == EV_SYN && ev.code == SYN_REPORT)
                got_frame = 1;
        }
        if (c.motion >= 0 && errno == ENODEV) lost = 1;

        if (lost) {
            ctl_close(&c);
            vpad_close(&vp);
            if (use_virtual) snprintf(g_vstatus, sizeof(g_vstatus), "aguardando o controle");
            memset(&in, 0, sizeof(in));
            memset(gyro, 0, sizeof(gyro));
        }
        if (got_frame) last_motion = now_s();
        int fresh = c.motion >= 0 && now_s() - last_motion < .15;
        ga_app_set_connection(&app, fresh, vp.fd >= 0);

        /* Sem controle, ainda roda o núcleo para a página refletir o estado. */
        if (got_frame || !fresh) {
            double now = now_s();
            in.dt = (float)(now - last_frame);
            last_frame = now;
            in.pitch_dps = fresh ? gyro[0] : 0;
            in.yaw_dps = fresh ? gyro[1] : 0;
            ga_app_process(&app, &in);
            if (c.motion >= 0) vpad_right_stick(&vp, app.out.rx, app.out.ry);
        }

        if (!terminal_output && strcmp(previous_status, g_vstatus)) {
            fprintf(stderr, "Controle virtual: %s\n", g_vstatus);
            snprintf(previous_status, sizeof(previous_status), "%s", g_vstatus);
        }
        if (terminal_output && t - last_draw > 1.0 / 15) {
            last_draw = t;
            draw(&app, &c, port);
        }
    }

    ga_webui_stop(&web);
    vpad_close(&vp);
    ctl_close(&c);
    return 0;
}

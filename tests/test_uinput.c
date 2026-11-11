/* Real Linux uinput roundtrip. Own synthetic device only; never grabs a real pad. */
#include "../src/linux/vpad.h"
#include "../src/core/gyro_aim.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

static int fails;
#define CHECK(c) do { if (!(c)) { fprintf(stderr,"FALHOU linha %d: %s\n",__LINE__,#c); fails++; } } while (0)

static int open_own_event(int fd)
{
    char name[128] = "", path[256];
    if (ioctl(fd, UI_GET_SYSNAME(sizeof(name)), name) < 0) return -1;
    snprintf(path, sizeof(path), "/sys/devices/virtual/input/%s", name);
    /* Kernel creation and device-node publication can complete separately. */
    for (int attempt = 0; attempt < 100; attempt++) {
        DIR *dir = opendir(path);
        if (dir) {
            struct dirent *de;
            while ((de = readdir(dir))) {
                if (strncmp(de->d_name, "event", 5)) continue;
                char device[300];
                snprintf(device, sizeof(device), "/dev/input/%.255s", de->d_name);
                int event = open(device, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
                if (event >= 0) { closedir(dir); return event; }
            }
            closedir(dir);
        }
        usleep(10000);
    }
    return -1;
}
static void drain(int fd)
{
    struct input_event events[64];
    while (read(fd, events, sizeof(events)) > 0) { }
}
static int value(int fd, int code)
{
    struct input_absinfo info = {0};
    CHECK(ioctl(fd, EVIOCGABS(code), &info) == 0);
    return info.value;
}
static void sync_frame(vpad *v)
{
    struct input_event ev = {.type=EV_SYN,.code=SYN_REPORT};
    vpad_forward(v, &ev, v->block_hotkeys);
}
static int is_pressed(int fd, int key)
{
    unsigned char bits[KEY_CNT / 8 + 1] = {0};
    CHECK(ioctl(fd, EVIOCGKEY(sizeof(bits)), bits) >= 0);
    return !!(bits[key / 8] & (1 << (key % 8)));
}
int main(void)
{
    vpad v;
    int r = vpad_open(&v, -1); /* synthetic ranges; no source device accessed */
    if (r) { fprintf(stderr, "uinput indisponível: %s\n", strerror(-r)); return 1; }
    int fd = open_own_event(v.fd);
    if (fd < 0) {
        fprintf(stderr,"Não foi possível ler o dispositivo de teste.\n");
        vpad_close(&v); return 1;
    }
    drain(fd);
    ga_config cfg; ga_config_default(&cfg);
    cfg.activation = GA_BTN_ALWAYS; cfg.smoothing = 0;
    cfg.gyro_deadzone = cfg.anti_deadzone = 0; cfg.sensitivity = 1;
    ga_state st; ga_state_reset(&st); ga_output out;
    ga_input in = {.yaw_dps=50,.pitch_dps=25,.dt=.004f};
    ga_update(&cfg, &st, &in, &out);
    vpad_right_stick(&v, out.rx, out.ry);
    struct pollfd p = {.fd=fd,.events=POLLIN};
    CHECK(poll(&p, 1, 100) > 0);
    CHECK(abs(value(fd,ABS_RX) + 16383) <= 1);
    CHECK(abs(value(fd,ABS_RY) + 6553) <= 1);
    drain(fd);
    /* A sensitivity change reaches the kernel event device. */
    cfg.sensitivity = 2;
    ga_update(&cfg, &st, &in, &out); vpad_right_stick(&v, out.rx, out.ry);
    CHECK(value(fd,ABS_RX) < -25000);
    /* Disabled gyro leaves a neutral input neutral. */
    cfg.enabled = 0; in.real_rx = in.real_ry = 0;
    ga_update(&cfg, &st, &in, &out); vpad_right_stick(&v, out.rx, out.ry);
    CHECK(value(fd,ABS_RX) == 0 && value(fd,ABS_RY) == 0);
    /* Physical right stick remains available while the gyro is disabled. */
    in.real_rx = .25f; in.real_ry = -.5f;
    ga_update(&cfg, &st, &in, &out); vpad_right_stick(&v, out.rx, out.ry);
    CHECK(abs(value(fd,ABS_RX) - 8191) <= 1);
    CHECK(abs(value(fd,ABS_RY) - 16383) <= 1);
    struct input_event ev = {.type=EV_KEY,.code=BTN_NORTH,.value=1};
    vpad_forward(&v, &ev, 0); sync_frame(&v);
    CHECK(is_pressed(fd,BTN_Y));
    vpad_hotkeys(&v, 1); CHECK(!is_pressed(fd,BTN_Y));
    ev.value = 0; vpad_forward(&v, &ev, 1); sync_frame(&v);
    vpad_hotkeys(&v, 0); CHECK(!is_pressed(fd,BTN_Y));
    /* Axis/button values really arrived as evdev events, not only in memory. */
    struct input_event events[64];
    ssize_t bytes = read(fd, events, sizeof(events));
    CHECK(bytes > 0);
    close(fd); vpad_close(&v);
    if (!fails) puts("uinput real: gyro, sensibilidade, pausa, analógico físico e atalhos passaram");
    return !!fails;
}

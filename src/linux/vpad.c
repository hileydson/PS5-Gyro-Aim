#include "vpad.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/uinput.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define STICK_MAX 32767

static void emit(int fd, int type, int code, int value)
{
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = (unsigned short)type;
    ev.code = (unsigned short)code;
    ev.value = value;
    if (write(fd, &ev, sizeof(ev)) < 0) { /* controle virtual sumiu: ignora */ }
}

static void abs_setup(int fd, int code, int min, int max, int flat)
{
    struct uinput_abs_setup a;
    memset(&a, 0, sizeof(a));
    a.code = (unsigned short)code;
    a.absinfo.minimum = min;
    a.absinfo.maximum = max;
    a.absinfo.flat = flat;
    ioctl(fd, UI_ABS_SETUP, &a);
}

int vpad_open(vpad *v, int src_fd)
{
    memset(v, 0, sizeof(*v));
    v->fd = -1;
    v->last_rx = v->last_ry = 1 << 30;

    for (int code = 0; code < ABS_CNT; code++) {
        v->src[code] = (struct input_absinfo){.minimum = 0, .maximum = 255};
        ioctl(src_fd, EVIOCGABS(code), &v->src[code]);
    }

    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return -errno;

    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    static const int keys[] = {BTN_A, BTN_B, BTN_X, BTN_Y, BTN_TL, BTN_TR, BTN_SELECT,
                               BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++)
        ioctl(fd, UI_SET_KEYBIT, keys[i]);

    static const int axes[] = {ABS_X, ABS_Y, ABS_RX, ABS_RY, ABS_Z, ABS_RZ, ABS_HAT0X, ABS_HAT0Y};
    for (size_t i = 0; i < sizeof(axes) / sizeof(axes[0]); i++)
        ioctl(fd, UI_SET_ABSBIT, axes[i]);

    /* mesmas faixas do driver xpad (Xbox 360) */
    abs_setup(fd, ABS_X, -32768, STICK_MAX, 128);
    abs_setup(fd, ABS_Y, -32768, STICK_MAX, 128);
    abs_setup(fd, ABS_RX, -32768, STICK_MAX, 0); /* sem flat: o gyro já trata */
    abs_setup(fd, ABS_RY, -32768, STICK_MAX, 0);
    abs_setup(fd, ABS_Z, 0, 255, 0);
    abs_setup(fd, ABS_RZ, 0, 255, 0);
    abs_setup(fd, ABS_HAT0X, -1, 1, 0);
    abs_setup(fd, ABS_HAT0Y, -1, 1, 0);

    struct uinput_setup us;
    memset(&us, 0, sizeof(us));
    us.id.bustype = BUS_USB;
    us.id.vendor = 0x045e;  /* Microsoft */
    us.id.product = 0x028e; /* Xbox 360 Controller */
    us.id.version = 0x0110;
    strncpy(us.name, "Gyro Aim Virtual Controller", UINPUT_MAX_NAME_SIZE - 1);

    if (ioctl(fd, UI_DEV_SETUP, &us) < 0 || ioctl(fd, UI_DEV_CREATE) < 0) {
        int e = errno;
        close(fd);
        return -e;
    }
    v->fd = fd;
    return 0;
}

void vpad_close(vpad *v)
{
    if (v->fd >= 0) {
        ioctl(v->fd, UI_DEV_DESTROY);
        close(v->fd);
    }
    v->fd = -1;
}

/* Converte o valor do DualSense para a faixa de destino. */
static int rescale(const struct input_absinfo *a, int value, int min, int max)
{
    long span = (long)a->maximum - a->minimum;
    if (span <= 0) return min;
    long r = min + ((long)value - a->minimum) * ((long)max - min) / span;
    return (int)(r < min ? min : r > max ? max : r);
}

void vpad_forward(vpad *v, const struct input_event *ev, int block_hotkeys)
{
    if (v->fd < 0) return;

    if (ev->type == EV_SYN && ev->code == SYN_REPORT) {
        emit(v->fd, EV_SYN, SYN_REPORT, 0);
        return;
    }

    if (ev->type == EV_KEY) {
        int out;
        /* hid-playstation usa NORTH = triângulo e WEST = quadrado;
         * no xpad, BTN_X (quadrado/esquerda) = 0x133 e BTN_Y (triângulo/cima) = 0x134. */
        switch (ev->code) {
        case BTN_SOUTH:  out = BTN_A; break;
        case BTN_EAST:   out = BTN_B; break;
        case BTN_WEST:   out = BTN_X; break;
        case BTN_NORTH:  out = BTN_Y; break;
        case BTN_TL: case BTN_TR: case BTN_SELECT: case BTN_START:
        case BTN_MODE: case BTN_THUMBL: case BTN_THUMBR:
            out = ev->code; break;
        default: return; /* L2/R2 digitais: os gatilhos analógicos já bastam */
        }
        if (block_hotkeys && ev->code == BTN_NORTH && ev->value) return;
        emit(v->fd, EV_KEY, out, ev->value);
        return;
    }

    if (ev->type == EV_ABS) {
        const struct input_absinfo *a = &v->src[ev->code < ABS_CNT ? ev->code : 0];
        switch (ev->code) {
        case ABS_X:
        case ABS_Y:
            emit(v->fd, EV_ABS, ev->code, rescale(a, ev->value, -32768, STICK_MAX));
            break;
        case ABS_Z:
        case ABS_RZ:
            emit(v->fd, EV_ABS, ev->code, rescale(a, ev->value, 0, 255));
            break;
        case ABS_HAT0X:
        case ABS_HAT0Y:
            if (block_hotkeys && ev->value) return;
            emit(v->fd, EV_ABS, ev->code, ev->value);
            break;
        default:
            break; /* ABS_RX/RY: o analógico direito sai por vpad_right_stick */
        }
    }
}

void vpad_right_stick(vpad *v, float rx, float ry)
{
    if (v->fd < 0) return;
    int x = (int)(rx * STICK_MAX);
    int y = (int)(-ry * STICK_MAX); /* no evdev, y positivo = baixo */
    if (x == v->last_rx && y == v->last_ry) return;
    v->last_rx = x;
    v->last_ry = y;
    emit(v->fd, EV_ABS, ABS_RX, x);
    emit(v->fd, EV_ABS, ABS_RY, y);
    emit(v->fd, EV_SYN, SYN_REPORT, 0);
}

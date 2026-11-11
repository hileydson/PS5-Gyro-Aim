/* Read vpad events through a pipe; no controller, grab or uinput required. */
#include "../src/linux/vpad.h"
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

static int read_events(int fd, struct input_event events[16])
{
    int n = 0;
    while (n < 16 && read(fd, &events[n], sizeof(events[n])) == sizeof(events[n])) n++;
    return n;
}
int main(void)
{
    int fds[2]; assert(pipe(fds) == 0);
    fcntl(fds[0], F_SETFL, O_NONBLOCK);
    vpad v = {.fd = fds[1]};
    struct input_event ev = {.type=EV_KEY,.code=BTN_NORTH,.value=1}, out[16];
    vpad_forward(&v, &ev, 0);
    assert(read_events(fds[0], out) == 1 && out[0].code == BTN_Y && out[0].value == 1);
    vpad_hotkeys(&v, 1);
    assert(read_events(fds[0], out) == 5 && out[0].code == BTN_Y && out[0].value == 0);
    ev = (struct input_event){.type=EV_ABS,.code=ABS_HAT0X,.value=1};
    vpad_forward(&v, &ev, 1); assert(read_events(fds[0], out) == 0);
    ev = (struct input_event){.type=EV_KEY,.code=BTN_WEST,.value=1};
    vpad_forward(&v, &ev, 1); assert(read_events(fds[0], out) == 0);
    /* Release triangle while shortcuts are active: no stuck button on resume. */
    ev = (struct input_event){.type=EV_KEY,.code=BTN_NORTH,.value=0};
    vpad_forward(&v, &ev, 1); assert(read_events(fds[0], out) == 0);
    vpad_hotkeys(&v, 0);
    assert(read_events(fds[0], out) == 5);
    assert(out[0].code == BTN_Y && out[0].value == 0);
    assert(out[1].code == BTN_X && out[1].value == 1);
    assert(out[2].code == ABS_HAT0X && out[2].value == 1);
    vpad_hotkeys(&v, 0); assert(read_events(fds[0], out) == 0);
    close(fds[0]); close(fds[1]);
    puts("vpad: atalhos e restauração de botões passaram");
    return 0;
}

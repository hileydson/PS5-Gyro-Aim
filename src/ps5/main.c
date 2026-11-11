/* PS5 companion daemon: queries title + serves profiles, never patches a process.
 * API signatures verified against etaHEN-Plugins Injector sample, commit
 * 6339554e4e3c92c0a655a37194c1152d6b82ad88. Hardware validation still required.
 */
#include "../core/app.h"
#include "../core/webui.h"
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

extern int sceSystemServiceGetAppIdOfRunningBigApp(void);
extern int sceSystemServiceGetAppTitleId(int app_id, char *title_id);
typedef struct {
    int32_t type, req_id, priority, msg_id, target_id, user_id, unk1, unk2;
    int32_t app_id, error_num, unk3;
    char use_icon;
    char message[1024], uri[1024], unkstr[1024];
} notification;
_Static_assert(sizeof(notification) == 0xc30, "notification ABI");
extern int sceKernelSendNotificationRequest(int, notification *, size_t, int);
static volatile sig_atomic_t quitting;
static void stop(int sig) { (void)sig; quitting = 1; }
static void notify(const char *text)
{
    notification req = {0};
    req.target_id = -1;
    snprintf(req.message, sizeof(req.message), "%s", text);
    sceKernelSendNotificationRequest(0, &req, sizeof(req), 0);
}
static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static int game_title(char title[64])
{
    int id = sceSystemServiceGetAppIdOfRunningBigApp();
    if (id < 0) { snprintf(title, 64, "default"); return 0; }
    char raw[256] = {0};
    if (sceSystemServiceGetAppTitleId(id, raw)) return -1;
    /* Do not misclassify system apps or a partial response as a game. */
    raw[sizeof(raw) - 1] = 0;
    if (strlen(raw) != 9 || (strncmp(raw, "PPSA", 4) && strncmp(raw, "CUSA", 4))) return -1;
    for (int i = 4; i < 9; i++) if (raw[i] < '0' || raw[i] > '9') return -1;
    snprintf(title, 64, "%s", raw);
    return 0;
}
int main(void)
{
    static ga_app app;
    static ga_webui web;
    ga_app_init(&app, "/data/gyroaim/profiles", "ps5", 0);
    app.notify = notify;
    int r = ga_webui_start(&web, 8090);
    if (r) { notify("Gyro Aim: porta 8090 indisponivel. Painel nao iniciado."); return 1; }
    signal(SIGINT, stop); signal(SIGTERM, stop); signal(SIGPIPE, SIG_IGN);
    notify("Gyro Aim: painel em http://IP-DO-PS5:8090. Modo de configuracao; sem hook de entrada.");
    double scan = 0;
    char candidate[64] = "", title[64];
    int stable = 0, failures = 0;
    while (!quitting) {
        double now = now_s();
        if (now - scan >= .5) {
            scan = now;
            if (!game_title(title)) {
                failures = 0;
                if (!strcmp(candidate, title)) stable++;
                else { snprintf(candidate, sizeof(candidate), "%s", title); stable = 1; }
                if (stable >= 2) ga_app_set_title(&app, candidate);
            } else {
                stable = 0;
                if (++failures >= 4) ga_app_set_title(&app, "default");
            }
        }
        ga_webui_poll(&web, &app);
        usleep(2000);
    }
    ga_webui_stop(&web);
    return 0;
}

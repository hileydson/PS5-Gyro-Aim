#include "native_protocol.h"
#include "profile.h"
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void add_profile(ga_native_snapshot *s, const char *title)
{
    for (uint32_t i = 0; i < s->profile_count; i++)
        if (!strcmp(s->profiles[i], title)) return;
    if (s->profile_count < GA_NATIVE_PROFILES)
        snprintf(s->profiles[s->profile_count++], 64, "%s", title);
}
static int profile_compare(const void *a, const void *b) { return strcmp(a, b); }
void ga_native_snapshot_build(const ga_app *a, ga_native_snapshot *s)
{
    memset(s, 0, sizeof(*s));
    s->magic = GA_NATIVE_MAGIC; s->version = GA_NATIVE_VERSION; s->size = sizeof(*s);
    s->revision = a->revision; s->has_profile = a->title_has_profile;
    s->paused = a->paused; s->input_ready = a->input_ready; s->output_ready = a->output_ready;
    s->can_undo = a->can_undo; s->calibrating = a->calibrating; s->testing = a->testing;
    s->cfg = a->cfg; s->rx = a->out.rx; s->ry = a->out.ry;
    snprintf(s->title, sizeof(s->title), "%s", a->title);
    snprintf(s->title_name, sizeof(s->title_name), "%s", a->title_name);
    snprintf(s->message, sizeof(s->message), "%s", a->message);
    snprintf(s->panel_url, sizeof(s->panel_url), "%s", a->panel_url);
    add_profile(s, GA_DEFAULT_PROFILE); add_profile(s, a->title);
    DIR *dir = opendir(a->profile_dir);
    if (!dir) return;
    struct dirent *de;
    while ((de = readdir(dir))) {
        size_t len = strlen(de->d_name);
        if (len <= 4 || len >= 68 || strcmp(de->d_name + len - 4, ".ini")) continue;
        char title[64], safe[64];
        memcpy(title, de->d_name, len - 4); title[len - 4] = 0;
        ga_title_sanitize(title, safe, sizeof(safe));
        if (!strcmp(title, safe)) add_profile(s, title);
    }
    closedir(dir);
    if (s->profile_count > 2)
        qsort(s->profiles + 2, s->profile_count - 2, sizeof(s->profiles[0]),
              profile_compare);
}
int ga_native_snapshot_valid(const ga_native_snapshot *s)
{
    if (s->magic != GA_NATIVE_MAGIC || s->version != GA_NATIVE_VERSION ||
        s->size != sizeof(*s) || s->profile_count > GA_NATIVE_PROFILES ||
        !memchr(s->title, 0, sizeof(s->title)) ||
        !memchr(s->title_name, 0, sizeof(s->title_name)) ||
        !memchr(s->panel_url, 0, sizeof(s->panel_url)) || !memchr(s->message, 0, sizeof(s->message)) || !isfinite(s->rx) || !isfinite(s->ry)) return 0;
    for (uint32_t i = 0; i < s->profile_count; i++)
        if (!memchr(s->profiles[i], 0, 64)) return 0;
    const float values[] = {s->cfg.sensitivity, s->cfg.anti_deadzone, s->cfg.vertical_ratio,
                           s->cfg.smoothing, s->cfg.gyro_deadzone, s->cfg.bias_pitch, s->cfg.bias_yaw};
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); i++) if (!isfinite(values[i])) return 0;
    return s->cfg.activation > GA_BTN_NONE && s->cfg.activation < GA_BTN_COUNT;
}

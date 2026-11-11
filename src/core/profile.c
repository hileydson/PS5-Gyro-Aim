#include "profile.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int ga_config_set(ga_config *cfg, const char *key, const char *value)
{
    char *end;
    if (strcmp(key, "activation") == 0) {
        ga_button b = ga_button_from_id(value);
        if (b == GA_BTN_NONE) return -1;
        cfg->activation = b;
        return 0;
    }

    double v = strtod(value, &end);
    if (end == value || *end || !isfinite(v)) return -1;

    if      (strcmp(key, "enabled") == 0)        cfg->enabled = v != 0;
    else if (strcmp(key, "toggle_mode") == 0)    cfg->toggle_mode = v != 0;
    else if (strcmp(key, "invert_x") == 0)       cfg->invert_x = v != 0;
    else if (strcmp(key, "invert_y") == 0)       cfg->invert_y = v != 0;
    else if (strcmp(key, "hotkeys_enabled") == 0) cfg->hotkeys_enabled = v != 0;
    else if (strcmp(key, "bias_pitch") == 0)     cfg->bias_pitch = (float)v;
    else if (strcmp(key, "bias_yaw") == 0)       cfg->bias_yaw = (float)v;
    else if (strcmp(key, "sensitivity") == 0)    cfg->sensitivity = (float)v;
    else if (strcmp(key, "anti_deadzone") == 0)  cfg->anti_deadzone = (float)v;
    else if (strcmp(key, "vertical_ratio") == 0) cfg->vertical_ratio = (float)v;
    else if (strcmp(key, "smoothing") == 0)      cfg->smoothing = (float)v;
    else if (strcmp(key, "gyro_deadzone") == 0)  cfg->gyro_deadzone = (float)v;
    else return -1;

    ga_config_clamp(cfg);
    return 0;
}

void ga_title_sanitize(const char *in, char *out, size_t n)
{
    if (!n) return;
    size_t j = 0;
    for (size_t i = 0; in && in[i] && j + 1 < n; i++) {
        char c = in[i];
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')
            out[j++] = c;
    }
    out[j] = '\0';
    if (j == 0)
        snprintf(out, n, "%s", GA_DEFAULT_PROFILE);
}

static void mkdir_p(const char *dir)
{
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            mkdir(tmp, 0777);
            *p = '/';
        }
    }
    mkdir(tmp, 0777);
}

static int load_file(const char *path, ga_config *cfg)
{
    FILE *f = fopen(path, "r");
    if (!f) return 0;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq || line[0] == '#') continue;
        *eq = '\0';
        char *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';
        ga_config_set(cfg, line, val);
    }
    fclose(f);
    return 1;
}

int ga_profile_load(const char *dir, const char *title, ga_config *cfg)
{
    char safe[64], path[512];
    ga_title_sanitize(title, safe, sizeof(safe));

    ga_config_default(cfg);
    snprintf(path, sizeof(path), "%s/%s.ini", dir, safe);
    if (load_file(path, cfg))
        return strcmp(safe, GA_DEFAULT_PROFILE) != 0;

    snprintf(path, sizeof(path), "%s/%s.ini", dir, GA_DEFAULT_PROFILE);
    load_file(path, cfg);
    return 0;
}

int ga_profile_save(const char *dir, const char *title, const ga_config *cfg)
{
    char safe[64], path[512], tmp[520];
    ga_title_sanitize(title, safe, sizeof(safe));
    mkdir_p(dir);
    snprintf(path, sizeof(path), "%s/%s.ini", dir, safe);
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE *f = fopen(tmp, "w");
    if (!f) return -errno;
    fprintf(f, "# PS5 Gyro Aim - perfil de %s\n", safe);
    fprintf(f, "enabled=%d\n", cfg->enabled);
    fprintf(f, "activation=%s\n", ga_button_id(cfg->activation));
    fprintf(f, "toggle_mode=%d\n", cfg->toggle_mode);
    fprintf(f, "sensitivity=%.2f\n", cfg->sensitivity);
    fprintf(f, "anti_deadzone=%.2f\n", cfg->anti_deadzone);
    fprintf(f, "vertical_ratio=%.2f\n", cfg->vertical_ratio);
    fprintf(f, "smoothing=%.2f\n", cfg->smoothing);
    fprintf(f, "gyro_deadzone=%.2f\n", cfg->gyro_deadzone);
    fprintf(f, "invert_x=%d\n", cfg->invert_x);
    fprintf(f, "invert_y=%d\n", cfg->invert_y);
    fprintf(f, "hotkeys_enabled=%d\n", cfg->hotkeys_enabled);
    fprintf(f, "bias_pitch=%.4f\nbias_yaw=%.4f\n", cfg->bias_pitch, cfg->bias_yaw);
    int err = ferror(f);
    if (fflush(f) != 0 || fsync(fileno(f)) != 0) err = 1;
    if (fclose(f) != 0) err = 1;
    if (err) { unlink(tmp); return -EIO; }
    /* grava em .tmp e renomeia: um desligamento no meio não corrompe o perfil */
    return rename(tmp, path) == 0 ? 0 : -errno;
}

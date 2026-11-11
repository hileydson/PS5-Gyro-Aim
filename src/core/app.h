/* Estado portátil: acessado apenas pelo loop da plataforma. */
#ifndef GA_APP_H
#define GA_APP_H
#include <stddef.h>
#include "gyro_aim.h"

typedef struct {
    ga_config cfg, undo_cfg, test_cfg;
    ga_state st;
    ga_output out;
    ga_input last_in;
    char title[64], profile_dir[256], platform[16];
    char title_name[128], title_source[24], panel_url[128];
    int remote_play; /* controle virtual destinado ao Chiaki; não implica sessão conectada */
    int title_has_profile, simulated;
    char message[256];
    unsigned message_seq, revision;
    int input_ready, output_ready, paused, can_undo, testing, save_error;
    int calibrating, calibration_samples;
    float calibration_time, mean_pitch, mean_yaw, m2_pitch, m2_yaw;
    void (*notify)(const char *msg);
} ga_app;

void ga_app_init(ga_app *app, const char *profile_dir, const char *platform, int simulated);
void ga_app_set_title(ga_app *app, const char *title);
void ga_app_set_connection(ga_app *app, int input_ready, int output_ready);
int ga_app_process(ga_app *app, const ga_input *in);
/* -1 inválido; -2 falha ao salvar; -3 perfil/revisão mudou. */
int ga_app_commit(ga_app *app, const ga_config *cfg);
int ga_app_set(ga_app *app, const char *key, const char *value);
int ga_app_action(ga_app *app, const char *action);
int ga_app_save_as_default(ga_app *app);
size_t ga_app_state_json(const ga_app *app, char *buf, size_t n);
#endif

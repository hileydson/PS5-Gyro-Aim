/*
 * Estado da aplicação compartilhado entre a plataforma (Linux/PS5) e a interface web.
 */
#ifndef GA_APP_H
#define GA_APP_H

#include <stddef.h>
#include "gyro_aim.h"

typedef struct {
    ga_config cfg;
    ga_state  st;
    ga_output out;
    ga_input  last_in;

    char title[64];          /* jogo em execução (title id) */
    int  title_has_profile;  /* 0 = usando o padrão até salvar algo */
    char profile_dir[256];
    int  simulated;          /* 1 = Linux: a interface pode trocar o "jogo" */
    char platform[16];
    char message[128];       /* última mensagem para o jogador */
    unsigned message_seq;

    void (*notify)(const char *msg); /* opcional: notificação na tela */
} ga_app;

void ga_app_init(ga_app *app, const char *profile_dir, const char *platform, int simulated);

/* Troca de jogo: carrega o perfil salvo daquele jogo. */
void ga_app_set_title(ga_app *app, const char *title);

/* Processa um quadro do controle. Salva o perfil se um atalho mudou algo. */
int  ga_app_process(ga_app *app, const ga_input *in);

/* Altera uma configuração e salva no perfil do jogo atual. 0 = ok. */
int  ga_app_set(ga_app *app, const char *key, const char *value);

/* Copia as configurações do jogo atual para o perfil padrão. */
int  ga_app_save_as_default(ga_app *app);

size_t ga_app_state_json(const ga_app *app, char *buf, size_t n);

#endif

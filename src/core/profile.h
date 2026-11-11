/*
 * Perfis por jogo: um arquivo <TITLE_ID>.ini por jogo dentro de profile_dir.
 * "default.ini" é usado quando o jogo ainda não tem perfil próprio.
 */
#ifndef GA_PROFILE_H
#define GA_PROFILE_H

#include <stddef.h>
#include "gyro_aim.h"

#define GA_DEFAULT_PROFILE "default"

/* Aplica uma chave (mesmos nomes do .ini e da interface web). 0 = ok. */
int  ga_config_set(ga_config *cfg, const char *key, const char *value);

/* Copia o title id só com [A-Za-z0-9_-]; vazio vira "default". */
void ga_title_sanitize(const char *in, char *out, size_t n);

/* 1 = perfil do jogo carregado, 0 = usou default.ini/padrões. */
int  ga_profile_load(const char *dir, const char *title, ga_config *cfg);
int  ga_profile_save(const char *dir, const char *title, const ga_config *cfg);

#endif

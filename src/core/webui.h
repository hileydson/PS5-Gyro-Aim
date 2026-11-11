/*
 * Servidor HTTP mínimo (sem threads) para a página de configuração.
 * Usa só sockets BSD/POSIX, então serve tanto no Linux quanto no PS5.
 */
#ifndef GA_WEBUI_H
#define GA_WEBUI_H

#include "app.h"

typedef struct {
    int fd;
    int port;
} ga_webui;

int  ga_webui_start(ga_webui *w, int port);   /* 0 = ok */
void ga_webui_poll(ga_webui *w, ga_app *app);  /* atende conexões pendentes, não bloqueia */
void ga_webui_stop(ga_webui *w);

extern const char GA_WEBUI_PAGE[];

#endif

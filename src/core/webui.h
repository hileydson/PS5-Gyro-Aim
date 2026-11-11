#ifndef GA_WEBUI_H
#define GA_WEBUI_H
#include "app.h"
#define GA_HTTP_CLIENTS 8
#define GA_HTTP_REQUEST 8192
#define GA_HTTP_RESPONSE 49152

typedef struct {
    int fd;
    double started;
    size_t used, length, sent;
    char request[GA_HTTP_REQUEST], response[GA_HTTP_RESPONSE];
} ga_http_client;
typedef struct {
    int fd, port;
    ga_http_client clients[GA_HTTP_CLIENTS];
} ga_webui;
int ga_webui_start(ga_webui *w, int port);
void ga_webui_poll(ga_webui *w, ga_app *app);
void ga_webui_stop(ga_webui *w);
extern const char GA_WEBUI_PAGE[];
extern const char GA_PRACTICE_PAGE[];
#endif

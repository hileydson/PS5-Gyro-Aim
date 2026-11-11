#include "webui.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

int ga_webui_start(ga_webui *w, int port)
{
    w->fd = -1;
    w->port = port;

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -errno;

    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(fd, 8) < 0) {
        int e = errno;
        close(fd);
        return -e;
    }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    w->fd = fd;
    return 0;
}

void ga_webui_stop(ga_webui *w)
{
    if (w->fd >= 0) close(w->fd);
    w->fd = -1;
}

static void send_all(int fd, const char *data, size_t len)
{
    while (len > 0) {
        ssize_t n = send(fd, data, len, MSG_NOSIGNAL);
        if (n <= 0) return;
        data += n;
        len -= (size_t)n;
    }
}

static void respond(int fd, int code, const char *type, const char *body, size_t len)
{
    char hdr[256];
    int n = snprintf(hdr, sizeof(hdr),
                     "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
                     "Cache-Control: no-store\r\nConnection: close\r\n\r\n",
                     code, code == 200 ? "OK" : code == 404 ? "Not Found" : "Bad Request",
                     type, len);
    send_all(fd, hdr, (size_t)n);
    send_all(fd, body, len);
}

static void url_decode(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '+') {
            *o++ = ' ';
        } else if (*s == '%' && isxdigit((unsigned char)s[1]) && isxdigit((unsigned char)s[2])) {
            char hex[3] = {s[1], s[2], 0};
            *o++ = (char)strtol(hex, NULL, 16);
            s += 2;
        } else {
            *o++ = *s;
        }
    }
    *o = '\0';
}

/* Chama fn(key, value) para cada par de "a=1&b=2". */
static int for_each_param(char *body, ga_app *app,
                          int (*fn)(ga_app *, const char *, const char *))
{
    int errors = 0;
    for (char *save = NULL, *tok = strtok_r(body, "&", &save); tok;
         tok = strtok_r(NULL, "&", &save)) {
        char *eq = strchr(tok, '=');
        if (!eq) { errors++; continue; }
        *eq = '\0';
        url_decode(tok);
        url_decode(eq + 1);
        if (fn(app, tok, eq + 1) != 0) errors++;
    }
    return errors;
}

static int set_title_param(ga_app *app, const char *key, const char *value)
{
    if (strcmp(key, "title") != 0) return -1;
    ga_app_set_title(app, value);
    return 0;
}

static void handle(int fd, ga_app *app)
{
    struct timeval tv = {0, 300000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    char req[8192];
    size_t got = 0;
    char *body = NULL;
    size_t content_len = 0;

    /* lê cabeçalho + corpo (pequeno) */
    while (got < sizeof(req) - 1) {
        ssize_t n = recv(fd, req + got, sizeof(req) - 1 - got, 0);
        if (n <= 0) break;
        got += (size_t)n;
        req[got] = '\0';
        if (!body) {
            char *end = strstr(req, "\r\n\r\n");
            if (!end) continue;
            body = end + 4;
            char *cl = strcasestr(req, "\r\nContent-Length:");
            if (cl) content_len = strtoul(cl + 17, NULL, 10);
        }
        if (body && (size_t)(req + got - body) >= content_len) break;
    }
    req[got] = '\0';
    if (!body) return;
    if (content_len < sizeof(req) && body + content_len <= req + got)
        body[content_len] = '\0';

    char method[8] = "", path[128] = "";
    sscanf(req, "%7s %127s", method, path);

    static char json[8192];
    if (strcmp(method, "GET") == 0 && strcmp(path, "/") == 0) {
        respond(fd, 200, "text/html; charset=utf-8", GA_WEBUI_PAGE, strlen(GA_WEBUI_PAGE));
        return;
    }
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/state") == 0) {
        size_t n = ga_app_state_json(app, json, sizeof(json));
        respond(fd, 200, "application/json", json, n);
        return;
    }
    if (strcmp(method, "POST") == 0) {
        int err = -1;
        if (strcmp(path, "/api/set") == 0)
            err = for_each_param(body, app, ga_app_set);
        else if (strcmp(path, "/api/title") == 0 && app->simulated)
            err = for_each_param(body, app, set_title_param);
        else if (strcmp(path, "/api/save_default") == 0)
            err = ga_app_save_as_default(app);

        if (err == 0) {
            size_t n = ga_app_state_json(app, json, sizeof(json));
            respond(fd, 200, "application/json", json, n);
        } else {
            respond(fd, 400, "text/plain", "erro", 4);
        }
        return;
    }
    respond(fd, 404, "text/plain", "nao encontrado", 14);
}

void ga_webui_poll(ga_webui *w, ga_app *app)
{
    if (w->fd < 0) return;
    for (int i = 0; i < 8; i++) {
        int c = accept(w->fd, NULL, NULL);
        if (c < 0) return;
        /* a conexão aceita herda O_NONBLOCK em alguns sistemas (BSD/PS5) */
        fcntl(c, F_SETFL, fcntl(c, F_GETFL, 0) & ~O_NONBLOCK);
        handle(c, app);
        close(c);
    }
}

#include "webui.h"
#include "profile.h"
#include "native_protocol.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static double now_s(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static void drop(ga_http_client *c)
{
    if (c->fd >= 0) close(c->fd);
    c->fd = -1;
}
int ga_webui_start(ga_webui *w, int port)
{
    memset(w, 0, sizeof(*w));
    w->fd = -1; w->port = port;
    for (int i = 0; i < GA_HTTP_CLIENTS; i++) w->clients[i].fd = -1;
    if (port < 1 || port > 65535) return -EINVAL;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -errno;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) || listen(fd, 8) ||
        fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0) {
        int e = errno; close(fd); return -e;
    }
    w->fd = fd;
    return 0;
}
void ga_webui_stop(ga_webui *w)
{
    if (w->fd >= 0) close(w->fd);
    w->fd = -1;
    for (int i = 0; i < GA_HTTP_CLIENTS; i++) drop(&w->clients[i]);
}
static void respond(ga_http_client *c, int code, const char *type, const char *body, size_t len)
{
    int n = snprintf(c->response, sizeof(c->response),
        "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %zu\r\n"
        "Cache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n"
        "X-Frame-Options: SAMEORIGIN\r\nConnection: close\r\n\r\n",
        code, code == 200 ? "OK" : code == 409 ? "Conflict" :
        code == 503 ? "Service Unavailable" : code == 404 ? "Not Found" : "Bad Request", type, len);
    if (n < 0 || len > sizeof(c->response) - (size_t)n) { drop(c); return; }
    memcpy(c->response + n, body, len);
    c->length = (size_t)n + len;
}
static void error(ga_http_client *c, int code, const char *msg)
{
    /* Mensagens constantes, sem dados do cliente. */
    char json[512];
    snprintf(json, sizeof(json), "{\"error\":\"%s\"}", msg);
    respond(c, code, "application/json; charset=utf-8", json, strlen(json));
}
static int decode(char *s)
{
    char *o = s;
    for (; *s; s++) {
        if (*s == '+') *o++ = ' ';
        else if (*s == '%') {
            if (!s[1] || !s[2] || !isxdigit((unsigned char)s[1]) || !isxdigit((unsigned char)s[2])) return -1;
            char h[3] = {s[1], s[2], 0};
            unsigned char v = (unsigned char)strtoul(h, NULL, 16);
            if (!v || v < 32 || v == 127) return -1;
            *o++ = (char)v; s += 2;
        } else *o++ = *s;
    }
    *o = 0;
    return 0;
}
/* Cabeçalhos já delimitados; rejeita duplicatas, inclusive Content-Length. */
static int header(const char *headers, const char *name, char *dst, size_t size)
{
    int found = 0;
    size_t len = strlen(name);
    for (const char *p = headers; *p;) {
        const char *end = strstr(p, "\r\n");
        if (!end) end = p + strlen(p);
        if ((size_t)(end - p) > len && !strncasecmp(p, name, len) && p[len] == ':') {
            if (found++) return -1;
            const char *v = p + len + 1;
            while (v < end && (*v == ' ' || *v == '\t')) v++;
            while (end > v && (end[-1] == ' ' || end[-1] == '\t')) end--;
            if ((size_t)(end - v) >= size) return -1;
            memcpy(dst, v, (size_t)(end - v)); dst[end - v] = 0;
        }
        p = strstr(p, "\r\n");
        if (!p) break;
        p += 2;
    }
    return found;
}

typedef struct { char *key, *value; } param;
static int params(char *body, param *ps, int max)
{
    int n = 0;
    if (!*body) return 0;
    while (body && *body) {
        char *next = strchr(body, '&');
        if (next) *next++ = 0;
        char *eq = strchr(body, '=');
        if (!eq || n == max) return -1;
        *eq++ = 0;
        if (decode(body) || decode(eq) || !*body) return -1;
        for (int i = 0; i < n; i++) if (!strcmp(ps[i].key, body)) return -1;
        ps[n++] = (param){body, eq};
        body = next;
    }
    return n;
}
static char *value(param *ps, int n, const char *key)
{
    for (int i = 0; i < n; i++) if (!strcmp(ps[i].key, key)) return ps[i].value;
    return NULL;
}
static void handle(ga_http_client *c, ga_app *a, char *body)
{
    char method[8], path[128], version[16], json[8192];
    if (sscanf(c->request, "%7s %127s %15s", method, path, version) != 3 || strcmp(version, "HTTP/1.1")) {
        error(c, 400, "Requisição inválida."); return;
    }
    int native = !strncmp(path, "/api/native", 11);
    if (!strcmp(method, "GET")) {
        if (!strcmp(path, "/api/native")) {
            ga_native_snapshot snapshot; ga_native_snapshot_build(a, &snapshot);
            respond(c, 200, "application/octet-stream", (const char *)&snapshot, sizeof(snapshot)); return;
        }
        if (!strcmp(path, "/") || !strcmp(path, "/compact"))
            respond(c, 200, "text/html; charset=utf-8", GA_WEBUI_PAGE, strlen(GA_WEBUI_PAGE));
        else if (!strcmp(path, "/practice"))
            respond(c, 200, "text/html; charset=utf-8", GA_PRACTICE_PAGE, strlen(GA_PRACTICE_PAGE));
        else if (!strcmp(path, "/api/state")) {
            size_t n = ga_app_state_json(a, json, sizeof(json));
            respond(c, 200, "application/json; charset=utf-8", json, n);
        } else error(c, 404, "Página não encontrada.");
        return;
    }
    if (strcmp(method, "POST")) { error(c, 400, "Método inválido."); return; }
    char guard[16], origin[256], host[256], type[128];
    int o = header(c->request, "Origin", origin, sizeof(origin));
    int h = header(c->request, "Host", host, sizeof(host));
    char expected[280]; snprintf(expected, sizeof(expected), "http://%s", h == 1 ? host : "");
    if (header(c->request, "X-GyroAim", guard, sizeof(guard)) != 1 || strcmp(guard, "1") ||
        header(c->request, "Content-Type", type, sizeof(type)) != 1 ||
        strcmp(type, "application/x-www-form-urlencoded") || h != 1 || o < 0 ||
        (o == 1 && strcmp(origin, expected))) {
        error(c, 400, "Origem ou formato inválido. Use o painel deste endereço."); return;
    }
    param ps[24]; int n = params(body, ps, 24);
    if (n < 0) { error(c, 400, "Parâmetros inválidos ou repetidos."); return; }
    char *title = value(ps, n, "title"), *revision = value(ps, n, "revision"), *end = NULL;
    unsigned long rev = revision ? strtoul(revision, &end, 10) : 0;
    if (!title || !revision || !*revision || *end || strcmp(title, a->title) || rev != a->revision) {
        error(c, 409, "O jogo ou os ajustes mudaram. Confira o perfil e tente novamente."); return;
    }
    int r = -1;
    if ((!strcmp(path, "/api/set") || !strcmp(path, "/api/native/set"))) {
        ga_config next = a->cfg; int count = 0; r = 0;
        for (int i = 0; i < n; i++) {
            if (!strcmp(ps[i].key, "title") || !strcmp(ps[i].key, "revision")) continue;
            count++;
            if (ga_config_set(&next, ps[i].key, ps[i].value)) r = -1;
        }
        if (!r) r = count ? ga_app_commit(a, &next) : -1;
    } else if ((!strcmp(path, "/api/action") || !strcmp(path, "/api/native/action")) && n == 3) {
        char *action = value(ps, n, "action");
        if (action) r = ga_app_action(a, action);
    } else if ((!strcmp(path, "/api/save_default") || !strcmp(path, "/api/native/save_default")) && n == 2) r = ga_app_save_as_default(a);
    else if ((!strcmp(path, "/api/title") || !strcmp(path, "/api/native/title")) &&
             (a->simulated || (native && !a->output_ready)) && n >= 3 && n <= 5) {
        char *next_title = value(ps, n, "next_title"), safe[64];
        ga_title_sanitize(next_title, safe, sizeof(safe));
        char *name = value(ps, n, "next_name"), *source = value(ps, n, "source");
        if (!name) name = "";
        if (!source) source = "manual";
        int known = 0;
        for (int i = 0; i < n; i++)
            if (!strcmp(ps[i].key, "title") || !strcmp(ps[i].key, "revision") ||
                !strcmp(ps[i].key, "next_title") || !strcmp(ps[i].key, "next_name") ||
                !strcmp(ps[i].key, "source")) known++;
        if (next_title && !strcmp(next_title, safe) && known == n &&
            (!strcmp(source, "manual") || !strcmp(source, "discovery") || !strcmp(source, "ps5_panel"))) {
            ga_app_set_title(a, safe);
            size_t length = strlen(name);
            if (length >= sizeof(a->title_name)) {
                length = sizeof(a->title_name) - 1;
                while (length && ((unsigned char)name[length] & 0xc0) == 0x80) length--;
            }
            memcpy(a->title_name, name, length); a->title_name[length] = 0;
            snprintf(a->title_source, sizeof(a->title_source), "%s", source);
            a->revision++;
            r = 0;
        }
    }
    if (r) error(c, r == -2 ? 503 : 400, r == -2 ? "Falha ao salvar. Ajuste anterior mantido." : "Ação ou valor inválido.");
    else if (native) {
        ga_native_snapshot snapshot; ga_native_snapshot_build(a, &snapshot);
        respond(c, 200, "application/octet-stream", (const char *)&snapshot, sizeof(snapshot));
    } else {
        size_t len = ga_app_state_json(a, json, sizeof(json));
        respond(c, 200, "application/json; charset=utf-8", json, len);
    }
}
/* Retorna sem esperar o cliente: uma conexão lenta não paralisa a mira. */
static void read_request(ga_http_client *c, ga_app *a)
{
    ssize_t n = recv(c->fd, c->request + c->used, sizeof(c->request) - 1 - c->used, 0);
    if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) { drop(c); return; }
    if (n > 0) c->used += (size_t)n;
    c->request[c->used] = 0;
    char *body = strstr(c->request, "\r\n\r\n");
    if (!body) {
        if (c->used == sizeof(c->request) - 1) error(c, 400, "Cabeçalho muito grande.");
        return;
    }
    *body = 0; body += 4;
    char length[32] = "", transfer[32];
    int found = header(c->request, "Content-Length", length, sizeof(length));
    char *end; unsigned long len = strtoul(length, &end, 10);
    size_t offset = (size_t)(body - c->request);
    if (found < 0 || (found && (!*length || *end || length[0] == '-')) ||
        header(c->request, "Transfer-Encoding", transfer, sizeof(transfer)) != 0 ||
        len >= sizeof(c->request) - offset || memchr(body, 0, c->used - offset)) {
        error(c, 400, "Tamanho ou corpo inválido."); return;
    }
    if (c->used - offset < len) { body[-4] = '\r'; return; }
    if (c->used - offset != len) { error(c, 400, "Corpo inesperado."); return; }
    handle(c, a, body);
}
void ga_webui_poll(ga_webui *w, ga_app *a)
{
    if (w->fd < 0) return;
    double now = now_s();
    for (int i = 0; i < GA_HTTP_CLIENTS; i++) {
        ga_http_client *c = &w->clients[i];
        if (c->fd < 0) {
            int fd = accept(w->fd, NULL, NULL);
            if (fd < 0) continue;
            if (fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK) < 0) { close(fd); continue; }
#ifdef SO_NOSIGPIPE
            int one = 1; setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
            c->fd = fd; c->used = c->length = c->sent = 0; c->started = now;
        }
        if (now - c->started > 2) { drop(c); continue; }
        if (!c->length) read_request(c, a);
        if (c->fd < 0 || !c->length) continue;
        ssize_t n = send(c->fd, c->response + c->sent, c->length - c->sent, MSG_NOSIGNAL);
        if (n > 0) c->sent += (size_t)n;
        if (c->sent == c->length || n == 0 ||
            (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) drop(c);
    }
}

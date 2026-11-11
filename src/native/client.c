#include "client.h"
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
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

int ga_native_request(int port, const ga_native_snapshot *current,
                      const char *route, const char *fields, ga_native_snapshot *result)
{
    if (port < 1 || port > 65535) return -1;
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
#ifdef SO_NOSIGPIPE
    int one = 1; setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET; addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK);
    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0 && errno != EINPROGRESS) { close(fd); return -1; }
    struct pollfd pollfd = {.fd=fd,.events=POLLOUT};
    int error = 0; socklen_t error_size = sizeof(error);
    if (poll(&pollfd, 1, 100) <= 0 || getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_size) || error) { close(fd); return -1; }
    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) & ~O_NONBLOCK);
    struct timeval tv = {0, 100000};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
    char request[2048], body[1024] = "";
    if (fields) {
        if (!current || !ga_native_snapshot_valid(current)) { close(fd); return -1; }
        /* Title IDs in snapshots are validated before interpolation. */
        for (const char *p = current->title; *p; p++)
            if (!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                  (*p >= '0' && *p <= '9') || *p == '_' || *p == '-')) { close(fd); return -1; }
        if (snprintf(body, sizeof(body), "title=%s&revision=%u&%s", current->title,
                     current->revision, fields) >= (int)sizeof(body)) { close(fd); return -1; }
    }
    int length = snprintf(request, sizeof(request), "%s %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\n"
        "X-GyroAim: 1\r\nContent-Type: application/x-www-form-urlencoded\r\n"
        "Content-Length: %zu\r\nConnection: close\r\n\r\n%s",
        fields ? "POST" : "GET", route, port, strlen(body), body);
    if (length <= 0 || length >= (int)sizeof(request)) { close(fd); return -1; }
    size_t sent = 0;
    while (sent < (size_t)length) {
        ssize_t n = send(fd, request + sent, (size_t)length - sent, MSG_NOSIGNAL);
        if (n <= 0) { close(fd); return -1; }
        sent += (size_t)n;
    }
    char response[8192]; size_t used = 0;
    for (;;) {
        ssize_t n = recv(fd, response + used, sizeof(response) - 1 - used, 0);
        if (n < 0) { close(fd); return -1; }
        if (!n) break;
        used += (size_t)n;
        if (used == sizeof(response) - 1) { close(fd); return -1; }
    }
    close(fd); response[used] = 0;
    int code = 0; sscanf(response, "HTTP/1.1 %d", &code);
    if (code != 200) return code ? -code : -1;
    char *payload = strstr(response, "\r\n\r\n");
    if (!payload) return -1;
    payload += 4;
    if (used - (size_t)(payload - response) != sizeof(*result)) return -1;
    ga_native_snapshot snapshot; memcpy(&snapshot, payload, sizeof(snapshot));
    if (!ga_native_snapshot_valid(&snapshot)) return -1;
    *result = snapshot;
    return 0;
}

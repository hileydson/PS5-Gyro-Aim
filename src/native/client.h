#ifndef GA_NATIVE_CLIENT_H
#define GA_NATIVE_CLIENT_H
#include "../core/native_protocol.h"
/* UI-only localhost RPC; never used from the input/game hook. */
int ga_native_request(int port, const ga_native_snapshot *current,
                      const char *route, const char *fields, ga_native_snapshot *result);
#endif

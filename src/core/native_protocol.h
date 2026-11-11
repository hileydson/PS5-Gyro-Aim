/* Versioned local IPC between the console application and its resident service.
 * Both targets use little-endian IEEE-754 x86-64. No pointers cross this interface.
 */
#ifndef GA_NATIVE_PROTOCOL_H
#define GA_NATIVE_PROTOCOL_H
#include "app.h"
#define GA_NATIVE_MAGIC 0x314e4147u
#define GA_NATIVE_VERSION 1u
#define GA_NATIVE_PROFILES 32
#define GA_NATIVE_TITLE_ID "PPSA94001"
typedef struct {
    uint32_t magic, version, size, revision;
    uint32_t has_profile, paused, input_ready, output_ready;
    uint32_t can_undo, calibrating, testing, profile_count;
    ga_config cfg;
    float rx, ry;
    char title[64], title_name[128], message[256], panel_url[128];
    char profiles[GA_NATIVE_PROFILES][64];
} ga_native_snapshot;
_Static_assert(sizeof(ga_native_snapshot) == 2732, "native IPC ABI changed: bump version");
void ga_native_snapshot_build(const ga_app *app, ga_native_snapshot *snapshot);
int ga_native_snapshot_valid(const ga_native_snapshot *snapshot);
#endif

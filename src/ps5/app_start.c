/* Native application entry point. ABI follows ps5-app-sdk app_crt.cpp
 * (BlackBearReloaded, GPL-3.0-or-later), expressed in C for this project.
 * Native titles receive process parameters, not payload_args_t.
 */
#include <stdint.h>
#include <stdlib.h>
typedef void (*initializer)(void);
extern void _init_env(void *parameters);
extern int main(int argc, char **argv);
extern initializer __preinit_array_start[] __attribute__((weak));
extern initializer __preinit_array_end[] __attribute__((weak));
extern initializer __init_array_start[] __attribute__((weak));
extern initializer __init_array_end[] __attribute__((weak));
extern initializer __fini_array_start[] __attribute__((weak));
extern initializer __fini_array_end[] __attribute__((weak));
static void initialize(initializer *first, initializer *last)
{ if (first && last) while (first != last) (*first++)(); }
static void finalize(void)
{ initializer *first=__fini_array_start,*last=__fini_array_end;if(first&&last)while(last!=first)(*--last)(); }
__attribute__((noreturn,visibility("default")))
void _start(void *parameters, initializer loader_teardown)
{
    int argc=*(const int *)parameters;
    char **argv=(char **)((uint8_t *)parameters+sizeof(uint64_t));
    _init_env(parameters);
    if(loader_teardown)atexit(loader_teardown);
    atexit(finalize);
    initialize(__preinit_array_start,__preinit_array_end);
    initialize(__init_array_start,__init_array_end);
    exit(main(argc,argv));
}

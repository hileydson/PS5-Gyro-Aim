/* Public payload libc has an optional kernel_mprotect callback. A native title
 * is only allowed to change its own mappings through the normal system API.
 * Never route this callback through privileged cross-process/kernel helpers.
 */
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
#include <unistd.h>
extern int sceKernelMprotect(void *, size_t, int);
int kernel_mprotect(int pid, intptr_t address, size_t length, int protection)
{
    if (pid != getpid()) { errno = EPERM; return -1; }
    int r = sceKernelMprotect((void *)address, length, protection);
    if (r < 0) { errno = EACCES; return -1; }
    return r;
}

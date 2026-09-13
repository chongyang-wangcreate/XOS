#include "types.h"
#include "usys.h"

void _exit(int status)
{
    SYS_CALL_DEF1(NR_EXIT, (uint64_t)status);
    while(1){
    }
}

int waitpid(int pid, int *status, int options)
{
    return SYS_CALL_DEF3(NR_WAITPID, (uint64_t)pid,
                         (uint64_t)status, (uint64_t)options);
}

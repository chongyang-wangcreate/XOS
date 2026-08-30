#include "types.h"
#include "usys.h"

int execve(const char *pathname,const char *argv[],const char* envp[])
{
    return SYS_CALL_DEF3(NR_EXECVE,(uint64)pathname,(uint64)argv,(uint64)envp);
}
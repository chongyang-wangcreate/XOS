#include "types.h"
#include "usys.h"

unsigned int sleep(unsigned int sec)
{
    SYS_CALL_DEF1(NR_SLEEP,sec);
    return 0;
}


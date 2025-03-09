#include "types.h"
#include "usys.h"

unsigned int fork()
{
   return  SYS_CALL_DEF0(NR_FORK);
}


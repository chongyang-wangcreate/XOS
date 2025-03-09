/********************************************************
    
    development started:2025
    All rights reserved
    author :wangchongyang

   Copyright (c) 2025 - 2028 wangchongyang

********************************************************/


#include "types.h"
#include "usys.h"
#include "printf.h"

unsigned int dup(int fd)
{
    
    int new_fd = SYS_CALL_DEF1(7,(uint64_t)fd);
    return new_fd;
}


/********************************************************
    
    development start:2024
    All rights reserved
    author :wangchongyang

    Copyright (c) 2024 - 2028 wangchongyang

    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

********************************************************/


#include "types.h"
#include "usys.h"

unsigned int write(int fd , char *buf, int size)
{
    return SYS_CALL_DEF3(NR_WRITE,fd, (uint64_t)buf,size);
}


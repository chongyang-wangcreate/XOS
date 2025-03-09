/********************************************************
    
    development start: 2025
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2025 ~- 2027 wangchongyang

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

int mkdir(const char *pathname,  int mode)
{
   return  SYS_CALL_DEF2(8,(uint64_t)pathname,mode);
}



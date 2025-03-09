/********************************************************
    
    development start:
    All rights reserved
    author :wangchongyang

   Copyright (c) 2024 ~ 2028 wangchongyang


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

********************************************************/


#include "types.h"
#include "arch64_timer.h"
#include "xos_sleep.h"
#include "printk.h"


/*
    What do we need to do?
    1.首先将seconds 转换成ticks
    2.获取当前cur,更改cur 状态
    3.加入timer 等待队列
    4.schedule 调用
        4.1 .切换地址空间
        4.2. 切换上下文
*/

int sys_nano_sleep(int seconds)
{
    uint32_t ticks;
    printk(PT_RUN,"%s:%d,seconds=%d\n\r",__FUNCTION__,__LINE__,seconds);
    ticks = second_to_ticks(seconds);
    xos_sleep_ticks(ticks);
    return 0;
}


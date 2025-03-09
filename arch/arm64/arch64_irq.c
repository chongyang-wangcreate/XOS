/********************************************************
    
    development timer: 2023
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com
   Copyright (c) 2023 ~ 2034 wangchongyang


   中午学习，晚上周末开发，坚持三年，实现第二次蜕变

********************************************************/


#include "types.h"
#include "setup_map.h"
#include "list.h"
#include "mem_layout.h"
#include "mmu.h"
#include "xos_debug.h"


void xos_enable_irq(void)
{
    asm volatile (
        "MSR DAIFClr, 0x2\n\t"
        );
}

 void xos_disable_irq(void)
{
    asm volatile (
        "MSR DAIFSet, 0x2\n\t"
        );
}

void xos_cli (void)
{
    asm("MSR DAIFSET, #2":::);
}

void xos_sti (void)
{
    asm("MSR DAIFCLR, #2":::);
}


/********************************************************

    Development Started: 2023-10-23 PM:21:45
    Copyright (C) 2023-2027 wangchongyang
    Email: rockywang599@gmail.com

   This program is licensed under the GPL v2 License. See LICENSE for more details.

   "When using, modifying, or distributing this code, 
   appropriate credit must be given to the original author. 
   You must include an acknowledgment of the original author in all copies or substantial portions of the software.
   
   2023.11.2
   中午学习，晚上周末开发，坚持三年，实现第二次蜕变

    从头开始写自己的系统，对我来说是一条艰难上升之路，中午学习思考，晚上到家敲代码
    我有大量工作要做，2023，2024 2025 大部分空闲时间基本会全部用来开发系统，辛苦自己了，但必须坚持下来。
    这一切会值得。

    Starting from simplicity
********************************************************/

#include "types.h"
#include "list.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "uart.h"
#include "mmu.h"
#include "mem_layout.h"
#include "common.h"
#include "string.h"
#include "printk.h"
#include "gic-v3.h"
#include "mmu.h"
#include "uart.h"
#include "boot_mem.h"


extern void * bss_start;
extern void * bss_end;

extern void set_virt_stack (void);

uart_t  *boot_uart_reg = NULL;

void boot_uart_init(uint32 *addr)
{
    int remain;
    boot_uart_reg = (uart_t*)addr;
    boot_uart_reg->uart_intbaud_reg = UART_CLK / (16 * UART_BITRATE);
    remain = UART_CLK % (16 * UART_BITRATE);
    boot_uart_reg->uart_fbaud_reg = (remain * 4 + UART_BITRATE / 2) / UART_BITRATE;

    boot_uart_reg->uart_ctrl_reg  |= (UARTCR_EN | UARTCR_RXE | UARTCR_TXE);
    boot_uart_reg->uart_lctrl_reg  |= UARTLCR_FEN;

}

void boot_uart_putc(int c)
{
    boot_uart_reg->uart_data_reg = c;
}


void boot_puts (char *s)
{
    while (*s != '\0') {
        boot_uart_putc(*s);
        s++;
    }
}


void boot_main (void)
{
    boot_uart_init((uint32*)UART0);
    boot_puts("xos boot_main\n");
    boot_mem_init();
    boot_init_early_map();

    set_virt_stack();
    boot_puts("Starting Kernel\n");
    kernel_init();
}

void other_boot_main()
{
    /*
        to do
        config ttbr0  ttbr1
        enable mmu
        loop  wait psci

        
    */
}



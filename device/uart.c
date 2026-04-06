/********************************************************
    
    development start:2023.11
    All rights reserved
    author :wangchongyang

    Copyright (c) 2023 - 2026 wangchongyang

    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
********************************************************/

#include "types.h"
#include "list.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "setup_map.h"
#include "pt_frame.h"
#include "interrupt.h"
#include "list.h"
#include "xos_mutex.h"
#include "arch_irq.h"
#include "tick_timer.h"
#include "bit_map.h"
#include "fs.h"
#include "xos_mutex.h"
#include "uart.h"
#include "xos_kern_def.h"
#include "xos_cache.h"
#include "schedule.h"
#include "console.h"
#include "mem_layout.h"
#include "printk.h"


/*

    20250103 定义 struct uart_queue, 标识队列深度，入队in_index ,出队out_index
    将buffer[index] 信息提取成一个struct uart_queue 结构

*/
uart_t  *boot_uart_reg = NULL;
con_uart_que  uart_que;
void uart_inqueue(char in_char);



uart_t  *uart_reg_base;
void uart_isr(void *desc);
int con_uart_inqueue(char ch_val);





void uart_loop()
{
    int loop_cnt = 1000;
    while(loop_cnt--);
}

void xos_uart_init()
{
    int remain;
    //uart_reg_base = (uart_t*)(UART0);
    uart_reg_base = (uart_t*)UART0_VIRT;
    uart_reg_base->uart_intbaud_reg = UART_CLK / (16 * UART_BITRATE);
    remain = UART_CLK % (16 * UART_BITRATE);
    uart_reg_base->uart_fbaud_reg = (remain * 4 + UART_BITRATE / 2) / UART_BITRATE;

    uart_reg_base->uart_ctrl_reg  |= (UARTCR_EN | UARTCR_RXE | UARTCR_TXE);
    
    uart_reg_base->uart_lctrl_reg  |= UARTLCR_FEN;

    
    uart_que.buf_depth = 256;
    uart_que.in_idx = 0;
    uart_que.out_idx = 0;
    mutext_init(&uart_que.que_lock);
}


void xos_uart_irq_init ()
{
    uart_reg_base->uart_intmask_reg =  UART_RXI;
    request_irq(33, uart_isr, 0, "arch_timer", NULL);
}

void xos_uart_putc (int c)
{
    while(uart_reg_base->uart_flag_reg& UARTFR_TXFF){
        uart_loop();
    }
    uart_reg_base->uart_data_reg = c;
}

void xos_uart_puts(char *s)
{
        while (*s != '\0') {
        xos_uart_putc(*s);
        s++;
    }

}
int xos_uart_getc (void)
{

    if (uart_reg_base->uart_flag_reg& UARTFR_RXFE){
         return -1;
    }
    return uart_reg_base->uart_data_reg;
}


// 将单个十六进制数字 (0-15) 转换为 ASCII 字符
void print_hex_digit(uint8_t num) {
    if (num < 10) {
        xos_uart_putc('0' + num);
    } else {
        xos_uart_putc('A' + num - 10);
    }
}

// 打印 64 位无符号整数的十六进制格式
void put_hex(uint64_t val) {
    int i;
    uint8_t digit;
    
    // 打印 "0x" 前缀
    xos_uart_puts("0x");
    
    // uint64 是 64 位，十六进制需要 16 位来表示
    // 我们从最高位开始打印，以确保不会出现前导零（除非值本身就是0）
    int started = 0; // 标记是否已经开始打印有效数字（用于去除前导零）
    
    for (i = 15; i >= 0; i--) {
        // 右移 (4 * i) 位并取低 4 位，得到当前十六进制位
        digit = (val >> (i * 4)) & 0xF;
        
        // 逻辑：跳过前导零，但如果所有位都是零，则至少打印一个零
        if (digit != 0) {
            started = 1; // 遇到第一个非零数字，开始打印
        }
        
        if (started) {
            print_hex_digit(digit);
        }
    }
    
    // 特殊情况：如果数值为 0，上面的循环不会打印任何东西，所以这里要补一个 0
    if (!started) {
        xos_uart_putc('0');
    }
}



int uart_read(char *data, unsigned short len)
{
    int ret;
    int read_len = 0;

    while(1){
        ret = xos_uart_getc();
        if(ret == -1){
            break;
        }
        printk(PT_RUN,"%s:%d,insert_code=%d\n\r",__FUNCTION__,__LINE__,ret);
        data[read_len] = ret;
        read_len++;
    }
    return read_len;
}


short uart_write(char *data, unsigned short len)
{
    int i;
    int write_len = 0;
    for (i = 0; i < len; ++i)
    {
        xos_uart_putc(data[write_len]);
        write_len++;
    }

    return write_len;
}


void uart_isr(void *desc)
{
    int i = 0;
    int ret_len = 0;
    char tmp_buf[256];
    ret_len = uart_read(tmp_buf, 1);
    while(ret_len){
        uart_inqueue(tmp_buf[i]);
        ret_len --;
        i += 1;
   }

}


void uart_inqueue(char in_char)
{
    ioq_putchar(&uart_que,in_char);
}


char uart_outqueue()
{
    return ioq_getchar(&uart_que);
}

/*
原方案
int uart_read(char *data, unsigned short len)
{
    int i;
    int ret;
    int read_len = 0;

    for(i = 0;i < len;i++)
    {
        ret = uart_getc();
        if(ret == -1){
            break;
        }
        printk(PT_DEBUG,"%s:%d,insert_code=%d\n\r",__FUNCTION__,__LINE__,ret);
        data[read_len] = ret;
        read_len++;
    }
    return read_len;
}


void uart_isr(void *desc)
{
    int index = 0;

    char buffer[256] = {0};
    do {
        
        if (uart_read(&buffer[index], 1) == 1)
        {
            uart_write(&buffer[index], 1);
            con_uart_inqueue(buffer[index]);
            index++;
        }
    }while (buffer[index -1] != '\r' && buffer[index -1] != '\n' && index < 256);
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);

    console_wake_process();

}
*/


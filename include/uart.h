#ifndef __UART_H__
#define __UART_H__

extern void uart_init (void *addr);
extern void uart_enable_rx();
extern void uart_putc (int c);
extern int  uart_getc(void);

#define UART_DR		0	// data register
#define UART_RSR	1	// receive status register/error clear register
#define UART_FR		6	// flag register
#define	UART_IBRD	9	// integer baud rate register
#define UART_FBRD	10	// Fractional baud rate register
#define UART_LCR	11	// line control register
#define UART_CR		12	// control register
#define UART_IMSC	14	// interrupt mask set/clear register
#define UART_MIS	16	// masked interrupt status register
#define	UART_ICR	17	// interrupt clear register
// bits in registers
#define UARTFR_TXFF	(1 << 5)	// tramit FIFO full
#define UARTFR_RXFE	(1 << 4)	// receive FIFO empty
#define	UARTCR_RXE	(1 << 9)	// enable receive
#define UARTCR_TXE	(1 << 8)	// enable transmit
#define	UARTCR_EN	(1 << 0)	// enable UART
#define UARTLCR_FEN	(1 << 4)	// enable FIFO
#define UART_RXI	(1 << 4)	// receive interrupt
#define UART_TXI	(1 << 5)	// transmit interrupt
#define UART_BITRATE 19200


#define UART0           0x09000000
#define UART_CLK        24000000    // Clock rate for UART

typedef struct uart_desc{
    unsigned int uart_data_reg;
    unsigned int uart_status_reg;
    unsigned int reserve[4];
    unsigned int uart_flag_reg;
    unsigned int reserve1[2];
    unsigned int uart_intbaud_reg;
    unsigned int uart_fbaud_reg;
    unsigned int uart_lctrl_reg;
    unsigned int uart_ctrl_reg;
    unsigned int reserve2;
    unsigned int uart_intmask_reg;
    unsigned int reserve3;
    unsigned int uart_int_stat_reg;
    unsigned int uart_intclr_reg;
    

}uart_t;
typedef struct console_uart_queue{
    char msg_buf[256];
    int  buf_depth;
    int  in_idx;
    int  out_idx;
    mutext_t que_lock;  //保护队列
    struct task_struct *tsk;
}con_uart_que;

extern void xos_uart_init();
extern void xos_uart_irq_init ();

extern con_uart_que  uart_que;
extern void console_wake_process();
extern char uart_outqueue();



#endif

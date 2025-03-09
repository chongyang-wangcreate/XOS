/********************************************************
    
    development started: 2025.1
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2025 ~ 2030 wangchongyang

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
#include "list.h"
#include "error.h"
#include "string.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "printk.h"
#include "fs.h"
#include "xos_kern_def.h"
#include "xos_cache.h"
#include "xos_char_dev.h"
#include "syscall.h"
#include "xos_mutex.h"
#include "tick_timer.h"
#include "task.h"
#include "uart.h"
#include "wait.h"
#include "schedule.h"
#include "cpu_desc.h"

/*
    2024.11.24 ：AM:3:11
    有时候真的感觉到非常累 ,很多事情看着简单实现起来并不容易，我自己跟自己说做成一件事耐心毅力非常重要
    虽然文件系统没完全实现，调试中出现问题，别灰心坚持一点点来，黎明前的黑暗。
    我这是牺牲个人健康回馈社会
    no rest, no vacation  Keep going for your dream！！！
    
    2024.12.21 当前的console 只会关联物理串口，console_read  ,console_wirte 实现的比较简单,实现队列出队入队


*/

typedef struct st_console_que{
    dlist_t        console_wait_list;
    xos_spinlock_t con_lock;
}console_que_t;

xcdev_t cdev_console;


console_que_t  wait_queue_head;

void console_wait_que_init(console_que_t *wait_que)
{
    list_init(&wait_que->console_wait_list);
    xos_spinlock_init(&wait_que->con_lock);
}

void console_wake_process()
{
    dlist_t *list_cur;
    struct task_struct *cur_tsk;
    dlist_t *wait_head = &wait_queue_head.console_wait_list;
    
    if(uart_que.msg_buf[uart_que.in_idx-1] == '\r'){
     printk(PT_DEBUG,"rrrrrrrrrrrrrr%s:%d\n\r",__FUNCTION__,__LINE__);
     list_for_each(list_cur, wait_head){
         cur_tsk = list_entry(list_cur, struct task_struct, wait_list);
         if(cur_tsk){
             printk(PT_DEBUG,"GGGGGGGGGGGGGGGG%s:%d\n\r",__FUNCTION__,__LINE__);
             
             wake_up_proc(cur_tsk);
             del_from_queue(list_cur);
             
             //clear queue
            }
        }
    }
}


/*
    每个设备都有自己独立的东西，所以一个xcdev_t 并不能覆盖全
    只能作为一个基类

*/
xcdev_t  console_dev;
int console_open (struct xos_inode *file_node, struct file *filp)
{
     printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);

     return 0;
}



long console_read (struct file *filp, char  *buf, size_t count, long *ppos)
{
    char *cur_buf = buf;
    int read_count = 0;
    if(count > 256 -filp->f_pos){
        count = 256 -filp->f_pos;
    }
    while (read_count < count) 
    {
        *cur_buf = uart_outqueue();
        read_count++;
        cur_buf++;
    }

     return read_count;
}

long console_write (struct file *filp, const char  *buf, size_t count, long *ppos)
{
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    return 0;
}

static struct xos_file_ops console_fops = {

    .open		= console_open,
    .read       = console_read,
    .write      = console_write,
};

/*
    当前设计方案是敲回车之后才唤醒shell 应用程序
    一次可能接收多个字符，避免频繁的休眠唤醒

    方案2： 每次输入一个字符，将字符放入buf 缓冲区之后，唤醒等待的shell 进程
    用户态进程去处理具体的字符串.

    方案3
    还需要支持特殊字符，所以这个函数还需要深度修改一下，封装成更规范的函数

    如果每次处理单个字符的化，用户态需要循环读取字符，读取之后继续休眠，直到接收到结束符退出
    循环，无数据阻塞休眠，读取失败也要继续休眠
*/
int con_uart_inqueue(char ch_val)
{
    
   int ret = -1;
   dlist_t *list_cur;
   dlist_t *wait_head = &wait_queue_head.console_wait_list;
   struct task_struct *cur_tsk;
   
   if(uart_que.in_idx >= uart_que.buf_depth){
        return ret;
   }
   uart_que.msg_buf[uart_que.in_idx] = ch_val;
   uart_que.in_idx++;
   if(uart_que.msg_buf[uart_que.in_idx-1] != '\n'){
    
   }
   printk(PT_DEBUG,"%s:%d,uart_que.msg_buf=%s\n\r",__FUNCTION__,__LINE__,uart_que.msg_buf);
   if(uart_que.msg_buf[uart_que.in_idx-1] == '\r'){
        list_for_each(list_cur, wait_head){
            cur_tsk = list_entry(list_cur, struct task_struct, wait_list);
            if(cur_tsk){
                del_from_queue(list_cur);
                wake_up_proc(cur_tsk);
            }
        }
        
   }
   
   return 0;
}


int con_uart_inqueue_new(char ch_val)
{
   int ret = -1;
   dlist_t *list_cur;
   dlist_t *wait_head = &wait_queue_head.console_wait_list;
   struct task_struct *cur_tsk;
   if(uart_que.in_idx >= uart_que.buf_depth){
        return ret;
   }
   uart_que.msg_buf[uart_que.in_idx] = ch_val;
   uart_que.in_idx++;
   list_for_each(list_cur, wait_head){
   cur_tsk = list_entry(list_cur, struct task_struct, wait_list);
   if(cur_tsk){
            wake_up_proc(cur_tsk);
        }
    }

   return 0;
}

char con_uart_outqueue_new(con_uart_que *que)
{
    struct task_struct  *cur_tsk =  current_task;
    if(uart_que.in_idx == 0){
        /*
            empty
        */
        uart_que.out_idx = 0;
        xos_spinlock(&wait_queue_head.con_lock);
        cur_tsk->state = TSTATE_PENDING;
        add_wait_queue(&cur_tsk->wait_list,&wait_queue_head.console_wait_list);
        del_from_cpu_runqueue(cur_cpuid(),cur_tsk);
        printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
        xos_unspinlock(&wait_queue_head.con_lock);
        schedule();
    }
    uart_que.out_idx++;
    uart_que.in_idx--;
    return que->msg_buf[uart_que.out_idx - 1];
}



static bool ioq_empty(con_uart_que* ioq) {
   return ioq->in_idx == ioq->out_idx;
}

static int32_t next_pos(int32_t pos) {
   return (pos + 1) % 256; 
}

static bool ioq_full(con_uart_que* ioq) {
   return next_pos(ioq->in_idx) == ioq->out_idx;
}


static void ioq_wait(con_uart_que* ioq) {
   struct task_struct  *cur_tsk =  current_task;
    xos_spinlock(&wait_queue_head.con_lock);
    ioq->tsk = cur_tsk;
    cur_tsk->state = TSTATE_PENDING;
    add_wait_queue(&cur_tsk->wait_list,&wait_queue_head.console_wait_list);
    del_from_cpu_runqueue(cur_cpuid(),cur_tsk);
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
    xos_unspinlock(&wait_queue_head.con_lock);
    schedule();
}

char ioq_getchar(con_uart_que* ioq) {

   struct task_struct  *cur_tsk =  current_task;
   while (ioq_empty(ioq)) {
    printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
       ioq_wait(ioq);
   }
   ioq->tsk = cur_tsk;
   printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
   char byte = ioq->msg_buf[ioq->out_idx];	 
   ioq->out_idx =  next_pos(ioq->out_idx); 
   return byte; 
}

void ioq_putchar(con_uart_que* ioq, char byte) {

   while (ioq_full(ioq)) {
      printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
      ioq_wait(ioq);
     
   }
   ioq->msg_buf[ioq->in_idx] = byte;      // 把字节放入缓冲区中
   ioq->in_idx = next_pos(ioq->in_idx); // 把写游标移到下一位置
//   if((byte == '\r')||(byte == '\n')||((int)byte == 128)||((int)byte == 127)
//    ||((int)byte == 13))
   {
        if(ioq->tsk != NULL){
            printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
            wake_up_proc(ioq->tsk);
        }
   }

}

void  console_driver_reigster()
{


}



void  console_driver_unreigster()
{


}



int console_init()
{
    int ret;
    devno_t devno = 10;
    unsigned baseminor = 0;
    char *dev_name = "console" ;
    ret = alloc_chrdev_region(&devno, baseminor, 1, dev_name);
    console_wait_que_init(&wait_queue_head);
//    ret = register_chrdev_region(devno,  baseminor,dev_name);
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    xcdev_init(&cdev_console, &console_fops);
    xcdev_add(&cdev_console, devno, 1);
    sys_mknod("/dev/console",S_IFCHR,devno);  //自动创建设备节点xnode

    return ret;
}

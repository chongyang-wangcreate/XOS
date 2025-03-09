#ifndef __TICK_TIMER_H__
#define __TICK_TIMER_H__

extern uint64 kernel_ticks;

typedef struct timer_struct{
    dlist_t t_list;
    u64  time_out; //超时时刻点
    u64  timer_val; //用户等级注册的等待时间
    struct task_struct *task; //当前定时器关联的任务
    void *arg;
    void (*timer_fun) (struct timer_struct *, void *); 

}xos_timer_t;

typedef void (*timer_fun) (struct timer_struct *, void *); 


#define kernel_tick_inc() (kernel_ticks++)
extern void handle_delay_task(int cpuid);
extern void  xos_init_timer(xos_timer_t *timer,u64 out_ticks,void *arg,timer_fun timer_call);
extern int xos_add_timer(xos_timer_t *timer);
extern void  xos_mod_timer(xos_timer_t *timer,uint64 timer_out);
extern void xos_timer_out_reg(xos_timer_t *timer,timer_fun  t_fun,void *arg);
extern void xos_check_timer();


#endif

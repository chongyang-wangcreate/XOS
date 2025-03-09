#ifndef __CPU_H__
#define __CPU_H__

#include "bit_map.h"
#include "spinlock.h"

#define CPU_NR         4

static inline u64 cur_cpuid() {
  u64 mpidr;
  asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
  return mpidr & 0xff;
}


/*
    基于统一的原则
    优先级值越小，优先级越高
*/
enum
{
    PRIO_ZERO,
    PRIO_ONE,
    PRIO_TWO,
    PRIO_DEFAULT,
    PRIO_FOUR,
    PRIO_FIVE,   
    PRIO_SEVEN,
    PRIO_EIGHT,
    PRIO_NIN,
    PRIO_TEN,
    PRIO_EL,
    PRIO_MAX,
};
/*
    定义成结构体方便以后扩展
*/
typedef struct prio_bitmap_st{
    int bit_num;
    char *bit_start;
}prio_bitmap;


typedef struct prio_bitmap{
    uint64 prio_group_0; /*0 ~63*/
    uint64 prio_group_1; /*64 ~127*/
    char group0_hit;    /*hit*63*/
    char group1_hit;   /*hit*63*/

}prio_bitmap_t;
typedef struct {
    prio_bitmap_t        task_prio_map;
    struct task_struct *task_table[PRIO_LOWEST + 1];
} tcb_map;


typedef struct struct_runqueue{
    dlist_t run_list;  
//    struct task_struct *cur_task;
//    tcb_map map_entity;
    
}runque_t;

typedef struct wait_queue{
    dlist_t wait_list;  /*信号量资源不足等待链表*/
    dlist_t event_list;
    dlist_t delay_list;
}wait_queue_t;
typedef struct struct_cpu_desc{
    int cpuid;
    int bind_nr; //当前核绑定数量
    int cur_pid;
    struct task_struct *cur_task;
//    prio_bitmap bitmap;
    bitmap_t run_bitmap;
    runque_t runqueue[PRIO_MAX];
    int run_count[PRIO_MAX];
    bitmap_t event_bitmap;
    bitmap_t delay_bitmap;
    wait_queue_t wait_que[PRIO_MAX];
    bitmap_t wait_bitmap;
    int bitmap_runque_start;
    int bitmap_waitque_start;
    dlist_t timer_list_head;
    xos_spinlock_t lock;
    tcb_map map_entity;

}cpu_desc_t;

extern cpu_desc_t cpu_array[CPU_NR];
extern void cpu_desc_init();



#endif

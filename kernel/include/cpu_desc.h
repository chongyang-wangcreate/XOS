#ifndef __CPU_H__
#define __CPU_H__

#include "bit_map.h"
#include "list.h"
#include "spinlock.h"


/*
    [23:16]‌	‌Aff3‌	‌第三级亲和性层级‌。
    通常用于标识 ‌Cluster（集群）‌ 或 ‌Socket（插槽）‌ 的更高层级分组。具体含义取决于 SoC 的实现。
    ‌[15:8]‌	‌Aff2‌	‌第二级亲和性层级‌。
    通常用于标识 ‌Cluster（集群）‌。例如，在多核 SoC 中，同一集群内的核心具有相同的 Aff2 值。
    ‌[7:0]‌	‌Aff1‌	‌第一级亲和性层级‌。
    通常用于标识 ‌Core（核心）‌ 在集群内的编号，或者在某些简单实现中直接标识核心 ID。

*/
#define CPU_NR         4

//#define MPIDR_MASK 0xff

#define MPIDR_HWID_MASK 0xff00ffffffUL

enum xos_cpu_boot_state {
    XOS_CPU_OFFLINE = 0,
    XOS_CPU_STARTING,
    XOS_CPU_ONLINE,
    XOS_CPU_FAILED,
};

typedef void (*xos_smp_call_func_t)(void *arg);

extern void xos_cpu_mark_starting(int cpuid);
extern void xos_cpu_mark_online(int cpuid);
extern void xos_cpu_mark_failed(int cpuid);
extern int xos_cpu_boot_state(int cpuid);

typedef void (*xos_smp_call_func_t)(void *arg);

extern int xos_mpidr_to_cpuid(u64 mpidr);
extern u64 xos_cpuid_to_mpidr(int cpuid);
extern int xos_cpu_possible_count(void);
extern void xos_cpu_mark_online(int cpuid);
extern void asm_secondary_entry(u64 mpidr);

extern void secondary_cpu_percpu_init(void);
extern void xos_smp_init(void);
extern void xos_smp_send_reschedule(int cpuid);
extern int xos_smp_call_function_on_cpu(int cpuid,
                                        xos_smp_call_func_t func,
                                        void *arg);

extern void secondary_cpu_percpu_init(void);
extern void xos_smp_init(void);
extern void xos_smp_send_reschedule(int cpuid);



static inline u64 read_mpidr_el1() {
  u64 mpidr;
  asm volatile("mrs %0, mpidr_el1" : "=r"(mpidr));
  return mpidr & MPIDR_HWID_MASK;
}

static inline u64 cur_cpuid(){
    int cpuid = xos_mpidr_to_cpuid(read_mpidr_el1());
    if(cpuid >= 0){
        return cpuid;
    }
    return read_mpidr_el1() & 0xff;
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
    PRIO_MAX = 32,
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
    uint64 mpidr;
    uint64 release_addr;  //spin table mode
    int possible;
    int cpu_online;
    volatile int boot_state;
    int boot_cpu;
    char enable_method[16];
    int bind_nr; //当前核绑定数量
    int cur_pid;
    struct task_struct *cur_task;
    struct task_struct *idle_task;
    xos_smp_call_func_t smp_call_func;
    void *smp_call_arg;
    volatile int smp_call_pending;
    volatile int smp_reschedule_pending;
    xos_spinlock_t smp_call_lock;
//    prio_bitmap bitmap;
    bitmap_t run_bitmap;
    runque_t runqueue[PRIO_MAX];
    int run_count[PRIO_MAX];
    bitmap_t rt_run_bitmap;
    runque_t rt_runqueue[PRIO_MAX];
    int rt_run_count[PRIO_MAX];
    bitmap_t normal_run_bitmap;
    runque_t normal_runqueue[PRIO_MAX];
    int normal_run_count[PRIO_MAX];
    bitmap_t event_bitmap;
    bitmap_t delay_bitmap;
    wait_queue_t wait_que[PRIO_MAX];
    bitmap_t wait_bitmap;
    int bitmap_runque_start;
    int bitmap_rt_runque_start;
    int bitmap_normal_runque_start;
    int bitmap_waitque_start;
    dlist_t timer_list_head;
    xos_spinlock_t lock;
    tcb_map map_entity;

}cpu_desc_t;

extern cpu_desc_t cpu_array[CPU_NR];
extern void cpu_desc_init();


#endif

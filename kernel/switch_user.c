#include "types.h"
#include "list.h"
#include "bit_map.h"
#include "setup_map.h"
#include "spinlock.h"
#include "tick_timer.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "mount.h"
#include "task.h"
#include "xos_kern_def.h"
#include "schedule.h"

extern void show_pt_regs(const struct pt_regs *regs);

void switch_to_user(struct pt_regs *regs,unsigned long sched_flags)
{
    /*
        检查是否需要抢占，查看need_switch 标志位
    */
//    show_pt_regs(regs);
}

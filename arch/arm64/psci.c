
#include "types.h"
#include "string.h"
#include "mem_layout.h"
#include "printk.h"
#include "list.h"
#include "cpu_desc.h"
#include "psci.h"
#include "barriers.h"
#include "arch64_timer.h"

#define CPU_ONLINE_WAIT_LOOPS 10000000UL


extern void secondary_entry(void);

#define PSCI_0_2_FN64_CPU_ON  0xc4000003UL
#define CPU_ONLINE_TIMEOUT_US 1000000UL


static long psci_cpu_on(uint64 target_mpidr,
                                uint64 entry_paddr,
                                uint64 context_id)
{
    register uint64 x0 asm("x0") = PSCI_0_2_FN64_CPU_ON;
    register uint64 x1 asm("x1") = target_mpidr;
    register uint64 x2 asm("x2") = entry_paddr;
    register uint64 x3 asm("x3") = context_id;

    asm volatile("hvc #0"
                 : "+r"(x0)
                 : "r"(x1), "r"(x2), "r"(x3)
                 : "memory");

    return (long)x0;   // 返回值：0 表示成功，负数表示错误码
}


static int cpu_wait_online(int cpuid)
{
    uint64 start;
    uint64 timeout_ticks;

    start = arch64_timer_get_counter();
    timeout_ticks = arch64_timer_us_to_ticks(CPU_ONLINE_TIMEOUT_US);
    if(timeout_ticks == 0){
        timeout_ticks = 1;
    }

    for(;;){
        if(xos_cpu_boot_state(cpuid) == XOS_CPU_ONLINE){
            dmb(ish);
            return 0;
        }
        if(xos_cpu_boot_state(cpuid) == XOS_CPU_FAILED){
            return -1;
        }
        if((arch64_timer_get_counter() - start) >= timeout_ticks){
            return -1;
        }
        asm volatile("yield" ::: "memory");
    }
}

static int cpu_request_start(int cpuid, uint64 boot_entry)
{
    long ret;

    xos_cpu_mark_starting(cpuid);
    if(strcmp(cpu_array[cpuid].enable_method, "psci") == 0){
        ret = psci_cpu_on(cpu_array[cpuid].mpidr, boot_entry, 0);
        if(ret != 0){
            printk(PT_ERROR, "psci cpu_on cpu%d failed ret=%ld\n\r",
                   cpuid, ret);
            xos_cpu_mark_failed(cpuid);
            return -1;
        }
    }else if(strcmp(cpu_array[cpuid].enable_method, "spin-table") == 0){
        dsb(sy);
        *((volatile uint64_t *)cpu_array[cpuid].release_addr) = boot_entry;
        dsb(sy);
        sev();
    }else{
        printk(PT_ERROR, "cpu%d unsupported enable-method=%s\n\r",
               cpuid, cpu_array[cpuid].enable_method);
        xos_cpu_mark_failed(cpuid);
        return -1;
    }
    return 0;
}

int xos_boot_secondary_cpus(void)
{
    int i;
    int failed = 0;
    int requested[CPU_NR];
    int requested_count = 0;
    int possible = xos_cpu_possible_count();
    uint64 boot_entry = (uint64)secondary_entry - VA_KERNEL_START;

    if(possible <= 1){
        return 0;
    }

    /* Phase 1: publish STARTING and release every secondary CPU. */
    for(i = 0; i < possible; i++) {
        if(!cpu_array[i].possible || cpu_array[i].boot_cpu ||
           cpu_array[i].boot_state == XOS_CPU_ONLINE){
            continue;
        }
        if(cpu_request_start(i, boot_entry) < 0){
            failed++;
            continue;
        }
        requested[requested_count++] = i;
    }

    /* Phase 2: wait until each secondary publishes ONLINE. */
    for(i = 0; i < requested_count; i++){
        int cpuid = requested[i];

        if(cpu_wait_online(cpuid) < 0){
            printk(PT_ERROR, "cpu%d online timeout\n\r", cpuid);
            xos_cpu_mark_failed(cpuid);
            failed++;
            continue;
        }
        printk(PT_WARRING, "cpu%d online acknowledged\n\r", cpuid);
    }

    return failed == 0 ? 0 : -1;
}


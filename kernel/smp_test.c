#include "types.h"
#include "list.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "fs.h"
#include "tick_timer.h"
#include "cpu_desc.h"
#include "printk.h"
#include "task.h"

static xos_spinlock_t smp_test_lock;
static uint64 smp_test_expected_mask;
static uint64 smp_test_worker_mask;
static uint64 smp_test_ipi_mask;
static int smp_test_pass_reported;

static void smp_test_maybe_report(void)
{
    unsigned long flags;
    uint64 workers;
    uint64 ipis;
    uint64 expected;
    int report = 0;

    flags = xos_spin_lock_irqsave(&smp_test_lock);
    workers = smp_test_worker_mask;
    ipis = smp_test_ipi_mask;
    expected = smp_test_expected_mask;
    if(!smp_test_pass_reported &&
       workers == expected && ipis == expected){
        smp_test_pass_reported = 1;
        report = 1;
    }
    xos_spin_unlock_irqrestore(&smp_test_lock, flags);

    if(report){
        printk(PT_WARRING,
               "[SMP-TEST] PASS expected=0x%lx workers=0x%lx ipis=0x%lx\n\r",
               expected, workers, ipis);
    }
}

static void smp_test_ipi_callback(void *arg)
{
    unsigned long flags;
    int cpuid = (int)cur_cpuid();

    (void)arg;
    flags = xos_spin_lock_irqsave(&smp_test_lock);
    smp_test_ipi_mask |= 1ULL << cpuid;
    xos_spin_unlock_irqrestore(&smp_test_lock, flags);
    smp_test_maybe_report();
}

static void smp_test_worker(void *arg)
{
    unsigned long flags;
    int cpuid = (int)cur_cpuid();

    (void)arg;
    flags = xos_spin_lock_irqsave(&smp_test_lock);
    smp_test_worker_mask |= 1ULL << cpuid;
    xos_spin_unlock_irqrestore(&smp_test_lock, flags);
    printk(PT_WARRING, "[SMP-TEST] cpu%d worker started\n\r", cpuid);
    smp_test_maybe_report();

    while(1){
        asm volatile("wfi" ::: "memory");
    }
}

int xos_smp_selftest(void)
{
    int cpuid;
    int possible = xos_cpu_possible_count();
    int ret = 0;

    xos_spinlock_init(&smp_test_lock);
    smp_test_expected_mask = 0;
    smp_test_worker_mask = 0;
    smp_test_ipi_mask = 0;
    smp_test_pass_reported = 0;

    for(cpuid = 0; cpuid < possible; cpuid++){
        if(cpu_array[cpuid].possible && cpu_array[cpuid].cpu_online){
            smp_test_expected_mask |= 1ULL << cpuid;
        }
    }

    printk(PT_WARRING, "[SMP-TEST] start expected=0x%lx\n\r",
           smp_test_expected_mask);

    for(cpuid = 0; cpuid < possible; cpuid++){
        if(!(smp_test_expected_mask & (1ULL << cpuid))){
            continue;
        }
        if(xos_smp_call_function_on_cpu(cpuid, smp_test_ipi_callback,
                                        NULL) != 0){
            printk(PT_ERROR, "[SMP-TEST] cpu%d IPI call failed\n\r", cpuid);
            ret = -1;
        }
        if(xos_thread_create_on_cpu(cpuid, 20,
                                    (unsigned long)smp_test_worker, 0) != 0){
            printk(PT_ERROR, "[SMP-TEST] cpu%d worker create failed\n\r",
                   cpuid);
            ret = -1;
        }
    }

    if(ret != 0){
        printk(PT_ERROR, "[SMP-TEST] FAIL setup\n\r");
    }
    return ret;
}



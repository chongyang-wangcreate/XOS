
#include "types.h"
#include "string.h"
#include "mem_layout.h"
#include "printk.h"
#include "list.h"
#include "cpu_desc.h"
#include "psci.h"
#include "barriers.h"

#define CPU_ONLINE_WAIT_LOOPS 10000000UL


extern void secondary_entry(void);

#define PSCI_0_2_FN64_CPU_ON  0xc4000003UL

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


int xos_boot_secondary_cpus(void)
{
    int i;
    unsigned long wait;
    long ret;
    int possible = xos_cpu_possible_count();
    uint64 boot_entry = (uint64)secondary_entry - VA_KERNEL_START;
    if (possible <= 1)
        return 0;

    /* 第一步：先向所有从核发起启动请求 */
    for (i = 0; i < possible; i++) {
        if (!cpu_array[i].possible ||  cpu_array[i].boot_cpu || cpu_array[i].cpu_online){
            continue;
        }
        if (strcmp(cpu_array[i].enable_method, "psci") == 0) {
            ret = psci_cpu_on(cpu_array[i].mpidr ,boot_entry,0);
            if (ret != 0) {
                printk(PT_ERROR,"psci cpu_on cpu%d failed ret=%ld\n\r",i,ret);
                continue;
            }
        } else if (strcmp(cpu_array[i].enable_method, "spin-table") == 0) {
            /* spin-table 方式：把入口地址写到释放地址 */
            __asm__ __volatile__("dsb sy" ::: "memory");
            *((volatile uint64_t *)cpu_array[i].release_addr) = boot_entry;
            __asm__ __volatile__("sev");
        } else {
            continue;
        }

        for(wait = 0; wait < CPU_ONLINE_WAIT_LOOPS; wait++){
            if(*(volatile int *)&cpu_array[i].cpu_online){
                dmb(ish);
                break;
            }
            asm volatile("yield" ::: "memory");
        }
        if(wait == CPU_ONLINE_WAIT_LOOPS){
            printk(PT_ERROR,"cpu%d online timeout\n\r",i);
        }
    }    

    
    return 0;

}

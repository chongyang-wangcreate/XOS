#include "types.h"
#include "list.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "fs.h"
#include "tick_timer.h"
#include "cpu_desc.h"
#include "task.h"
#include "process.h"
#include "printk.h"

#define PROCESS_TEST_EXIT_CODE 42
#define PROCESS_TEST_LOOPS 32

static volatile int process_test_release;
static volatile int process_test_exit_code;

static void process_test_child(void *arg)
{
    (void)arg;
    printk(PT_WARRING, "[PROCESS-TEST] child cpu%d started\n\r",
           (int)cur_cpuid());
    while(!process_test_release){
        asm volatile("yield" ::: "memory");
    }
    printk(PT_WARRING, "[PROCESS-TEST] child cpu%d exiting\n\r",
           (int)cur_cpuid());
    do_sys_exit(process_test_exit_code);
}

int xos_process_selftest(void)
{
    int loop;
    int status = -1;
    int pid;
    int waited;

    printk(PT_WARRING, "[PROCESS-TEST] start\n\r");
    for(loop = 0; loop < PROCESS_TEST_LOOPS; loop++){
        process_test_release = 0;
        process_test_exit_code = PROCESS_TEST_EXIT_CODE + loop;
        pid = xos_process_thread_create(2,
                                        (unsigned long)process_test_child, 0);
        if(pid < 0){
            printk(PT_ERROR,
                   "[PROCESS-TEST] FAIL create loop=%d\n\r", loop);
            return -1;
        }

        waited = do_sys_waitpid(pid, &status, XOS_WNOHANG);
        if(waited != 0){
            printk(PT_ERROR,
                   "[PROCESS-TEST] FAIL WNOHANG loop=%d ret=%d\n\r",
                   loop, waited);
            return -1;
        }

        process_test_release = 1;
        asm volatile("dmb ish" ::: "memory");
        waited = do_sys_waitpid(pid, &status, 0);
        if(waited != pid || status != PROCESS_TEST_EXIT_CODE + loop){
            printk(PT_ERROR,
                   "[PROCESS-TEST] FAIL loop=%d pid=%d waited=%d status=%d\n\r",
                   loop, pid, waited, status);
            return -1;
        }
    }

    printk(PT_WARRING,
           "[PROCESS-TEST] PASS loops=%d last_pid=%d status=%d\n\r",
           PROCESS_TEST_LOOPS, waited, status);
    return 0;
}

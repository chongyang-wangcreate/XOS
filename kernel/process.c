#include "types.h"
#include "list.h"
#include "string.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "fs.h"
#include "xos_cache.h"
#include "mem_layout.h"
#include "mmu.h"
#include "setup_map.h"
#include "xos_page.h"
#include "error.h"
#include "printk.h"
#include "tick_timer.h"
#include "task.h"
#include "schedule.h"
#include "xos_pid.h"
#include "xos_sleep.h"
#include "process.h"

#define XOS_PAGE_TABLE_ENTRIES 512

static int process_entry_is_table(uint64 entry)
{
    return ((entry & (PT_ENTRY_TABLE | PT_ENTRY_VALID)) ==
            (PT_ENTRY_TABLE | PT_ENTRY_VALID));
}

static void process_release_files(struct task_struct *task)
{
    int fd;

    for(fd = 0; fd < MAX_FILE_NR; fd++){
        struct file *filp = task->files_set.file_set[fd];

        if(filp == NULL){
            continue;
        }
        xos_spinlock(&filp->f_lock);
        if(filp->ref_count > 0){
            filp->ref_count--;
        }
        if(filp->f_count > 0){
            filp->f_count--;
        }
        xos_unspinlock(&filp->f_lock);
        task->files_set.file_set[fd] = NULL;
    }
    memset(task->files_set.fd_set, 0, sizeof(task->files_set.fd_set));
}

static void process_release_page_tables(pgd_t *task_pgd)
{
    int pgd_idx;

    if(task_pgd == NULL){
        return;
    }

    for(pgd_idx = 0; pgd_idx < XOS_PAGE_TABLE_ENTRIES; pgd_idx++){
        pgd_t pgd_entry = task_pgd[pgd_idx];
        pmd_t *pmd_base;
        int pmd_idx;

        if(!process_entry_is_table(pgd_entry)){
            continue;
        }
        pmd_base = (pmd_t *)P2V(pgd_entry & PG_4k_ADDR_MASK);
        for(pmd_idx = 0; pmd_idx < XOS_PAGE_TABLE_ENTRIES; pmd_idx++){
            pmd_t pmd_entry = pmd_base[pmd_idx];
            pte_t *pte_base;
            int pte_idx;

            if(!process_entry_is_table(pmd_entry)){
                continue;
            }
            pte_base = (pte_t *)P2V(pmd_entry & PG_4k_ADDR_MASK);
            for(pte_idx = 0; pte_idx < XOS_PAGE_TABLE_ENTRIES; pte_idx++){
                pte_t pte = pte_base[pte_idx];

                if(pte & PT_ENTRY_VALID){
                    xos_page_put(pte & PG_4k_ADDR_MASK);
                    pte_base[pte_idx] = 0;
                }
            }
            pmd_base[pmd_idx] = 0;
            xos_free_page(pte_base);
        }
        task_pgd[pgd_idx] = 0;
        xos_free_page(pmd_base);
    }
}

void process_release_address_space(struct task_struct *task)
{
    pgd_t *task_pgd;
    struct mm_struct *mm;
    struct vm_area_struct *vma;

    if(task == NULL){
        return;
    }

    if(task->resource_flags & TASK_RESOURCE_PGD){
        task_pgd = task->task_pgd;
        task->task_pgd = NULL;
        task->resource_flags &= ~TASK_RESOURCE_PGD;
        process_release_page_tables(task_pgd);
        xos_free_page(task_pgd);
    }

    mm = task->mm;
    if(mm == NULL){
        return;
    }

    vma = mm->mmap;
    while(vma != NULL){
        struct vm_area_struct *next = vma->vm_next;

        xos_kfree(vma);
        vma = next;
    }
    mm->mmap = NULL;

    if(task->resource_flags & TASK_RESOURCE_MM){
        task->mm = NULL;
        task->resource_flags &= ~TASK_RESOURCE_MM;
        xos_kfree(mm);
    }
}

static void process_reap_zombie(struct task_struct *task)
{
    void *kstack;

    if(task == NULL){
        return;
    }

    task_unregister(task);
    process_release_files(task);
    process_release_address_space(task);
    free_pid(task->pid);
    task->pid = 0;

    if(task->resource_flags & TASK_RESOURCE_KSTACK){
        kstack = task->kstack;
        task->kstack = NULL;
        task->resource_flags &= ~TASK_RESOURCE_KSTACK;
        xos_free_page(kstack);
    }
    if(task->resource_flags & TASK_RESOURCE_TASK){
        task->resource_flags &= ~TASK_RESOURCE_TASK;
        xos_free_page(task);
    }
}

void do_sys_exit(int status)
{
    struct task_struct *task = current_task;

    if(task == NULL){
        while(1){
            asm volatile("wfi" ::: "memory");
        }
    }

    task->exit_code = status;
    task->state = TSTATE_ZOMBIE;
    task->need_switch = 0;
    del_from_cpu_runqueue(task->cpuid, task);
    schedule();
    /*Normally, it wouldn’t run to this point*/
    while(1){
        schedule();
        asm volatile("wfi" ::: "memory");
    }
}

int do_sys_waitpid(int pid, int *status, int options)
{
    struct task_struct *parent = current_task;

    if(parent == NULL || pid == 0 || pid < -1 ||
       (options & ~XOS_WNOHANG) != 0){
        return -EINVAL;
    }

    while(1){
        struct task_struct *child;
        int has_child = 0;
        int child_pid;
        int child_status;

        child = task_find_waitable_child(parent, pid, &has_child);
        if(child != NULL && task_claim_zombie(child)){
            child_pid = child->pid;
            child_status = child->exit_code;
            if(status != NULL){
                *status = child_status;
            }
            process_reap_zombie(child);
            return child_pid;
        }
        if(!has_child){
            return -ECHILD;
        }
        if(options & XOS_WNOHANG){
            return 0;
        }
        xos_sleep_ticks(1);
    }
}

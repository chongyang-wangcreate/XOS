#include "types.h"
#include "string.h"
#include "list.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "fs.h"
#include "tick_timer.h"
#include "mem_layout.h"
#include "mmu.h"
#include "pt_frame.h"
#include "task.h"
#include "schedule.h"
#include "xos_cache.h"
#include "xos_page.h"
#include "user_map.h"
#include "printk.h"

#define XOS_EXEC_MAX_ARGS 16
#define XOS_EXEC_MAX_STR  256

extern void init_mm(struct mm_struct *mm);
extern int create_process_vma(struct task_struct *cur_task);
extern void vma_space_maps(struct task_struct *t);
extern uint64_t get_user_entry(void);

static void exec_flush_user_tlb(void)
{
    asm volatile("dsb ishst" ::: "memory");
    asm volatile("tlbi vmalle1is" ::: "memory");
    asm volatile("dsb ish" ::: "memory");
    asm volatile("isb" ::: "memory");
}

static int exec_arg_count(char *const arg[])
{
    int i = 0;

    if (arg == NULL) {
        return 0;
    }

    while (arg[i] != NULL) {
        if (i >= XOS_EXEC_MAX_ARGS) {
            return -1;
        }
        i++;
    }
    return i;
}

static char *exec_strdup_user(const char *src)
{
    int len;
    char *dst;

    if (src == NULL) {
        return NULL;
    }

    len = strlen(src);
    if (len <= 0 || len >= XOS_EXEC_MAX_STR) {
        return NULL;
    }

    dst = xos_kmalloc(len + 1);
    if (dst == NULL) {
        return NULL;
    }
    strcpy(dst, src);
    return dst;
}

static void exec_free_strv(char **strv)
{
    int i;

    if (strv == NULL) {
        return;
    }

    for (i = 0; strv[i] != NULL; i++) {
        xos_kfree(strv[i]);
    }
    xos_kfree(strv);
}

static char **exec_copy_strv(char *const src[])
{
    int count;
    int i;
    char **dst;

    count = exec_arg_count(src);
    if (count < 0) {
        return NULL;
    }

    dst = xos_kmalloc(sizeof(char *) * (count + 1));
    if (dst == NULL) {
        return NULL;
    }
    memset(dst, 0, sizeof(char *) * (count + 1));

    for (i = 0; i < count; i++) {
        dst[i] = exec_strdup_user(src[i]);
        if (dst[i] == NULL) {
            exec_free_strv(dst);
            return NULL;
        }
    }
    return dst;
}

static void exec_free_vmas(struct mm_struct *mm)
{
    struct vm_area_struct *vma;
    struct vm_area_struct *next;

    if (mm == NULL) {
        return;
    }

    for (vma = mm->mmap; vma != NULL; vma = next) {
        next = vma->vm_next;
        xos_kfree(vma);
    }
    mm->mmap = NULL;
    mm->mmap_count = 0;
}

static int exec_rebuild_builtin_image(struct task_struct *task)
{
    pgd_t *new_pgd;

    if (task == NULL || task->mm == NULL) {
        return -1;
    }

    new_pgd = xos_get_free_page(0, 1);
    if (new_pgd == NULL) {
        return -1;
    }
    memset(new_pgd, 0, PAGE_SIZE);

    exec_free_vmas(task->mm);
    task->task_pgd = new_pgd;
    task->mm->mm_pgd = new_pgd;

    init_mm(task->mm);
    if (create_process_vma(task) < 0) {
        return -1;
    }
    vma_space_maps(task);

    set_ttbr0_el1((uint64)V2P(task->task_pgd));
    exec_flush_user_tlb();
    return 0;
}

int do_sys_execve(const char *pathname, char *const argv[], char *const envp[],
                  struct pt_regs *regs)
{
    char *kpath;
    char **kargv;
    char **kenvp;
    struct task_struct *task;
    uint64_t user_entry;

    if (pathname == NULL || regs == NULL) {
        return -1;
    }

    kpath = exec_strdup_user(pathname);
    if (kpath == NULL) {
        return -1;
    }
    kargv = exec_copy_strv(argv);
    kenvp = exec_copy_strv(envp);
    if (kargv == NULL || kenvp == NULL) {
        exec_free_strv(kargv);
        exec_free_strv(kenvp);
        xos_kfree(kpath);
        return -1;
    }

    task = get_current_task();
    if (exec_rebuild_builtin_image(task) < 0) {
        exec_free_strv(kargv);
        exec_free_strv(kenvp);
        xos_kfree(kpath);
        return -1;
    }

    user_entry = get_user_entry();
    task->user_entry = (task_fun)user_entry;
    strcpy(task->user_path, kpath);
    regs->pc = user_entry;
    regs->sp = USER_STACK_TOP;
    regs->pstate = PSR_MODE_EL0t;
    regs->regs[0] = 0;

    exec_free_strv(kargv);
    exec_free_strv(kenvp);
    xos_kfree(kpath);
    return 0;
}

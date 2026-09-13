/********************************************************

    Development Started: 2025-1-28 21:45
    Copyright (C) 2025-2027 wangchongyang
    Email: rockywang599@gmail.com

   This program is licensed under the GPL v2 License. See LICENSE for more details.

   "When using, modifying, or distributing this code, 
   appropriate credit must be given to the original author. 
   You must include an acknowledgment of the original author in all copies or substantial portions of the software.

********************************************************/

#include "types.h"
#include "error.h"
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
#include "xos_file.h"
#include "user_map.h"
#include "printk.h"
#include "syscall.h"
#include "xos_fcntl.h"
#include "xos_kern_def.h"
#include "uio.h"

#define XOS_EXEC_MAX_ARGS 16
#define XOS_EXEC_MAX_STR  256

#define XOS_EXEC_USER_HEAP_SIZE 0x10000000UL
#define XOS_EXEC_INV_PHY_ADDR  ((uint64)-1)

#define ELF64_MAGIC0 0x7f
#define ELF64_MAGIC1 'E'
#define ELF64_MAGIC2 'L'
#define ELF64_MAGIC3 'F'

#define ELF64_ET_EXEC 2
#define ELF64_PT_LOAD 1
#define ELF64_PF_X 1
#define ELF64_PF_W 2
/*
    2025.11.14 20:36

    adding elf header files and related functions
*/
typedef struct elf64_ehdr {
    unsigned char e_ident[16];
    uint16_t e_type;
    uint16_t e_machine;
    uint32_t e_version;
    uint64_t e_entry;
    uint64_t e_phoff;
    uint64_t e_shoff;
    uint32_t e_flags;
    uint16_t e_ehsize;
    uint16_t e_phentsize;
    uint16_t e_phnum;
    uint16_t e_shentsize;
    uint16_t e_shnum;
    uint16_t e_shstrndx;
} elf64_ehdr_t;

typedef struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
} elf64_phdr_t;


#define ELF_MAXPHNUM  128

extern void init_mm(struct mm_struct *mm);
extern int create_process_vma(struct task_struct *cur_task);
extern void vma_space_maps(struct task_struct *t);
extern uint64_t get_user_entry(void);
extern int create_vma(struct task_struct *tsk, unsigned long vm_start,
                      unsigned long vm_end, unsigned long phy_addr,
                      unsigned long vm_flags);

static void exec_free_vmas(struct mm_struct *mm);
static void exec_flush_user_tlb(void);

/*static inline uint64_t exec_align_down(uint64_t v,align_size)
{
    return (v & ~(align_size - 1));
}*/

static inline uint64_t align_page_down(uint64_t v)
{
    return (v & ~(PAGE_SIZE - 1));
}

static inline uint64_t align_page_up(uint64_t v)
{
    return ALIGN_UP(v, PAGE_SIZE);
}

static int exec_is_builtin_path(const char *path)
{
    if (path == NULL) {
        return 0;
    }
    return strcmp(path, "builtin") == 0 || strcmp(path, "/builtin") == 0 ||
           strcmp(path, "/shell") == 0;
}



static int exec_read(int fd, uint64_t off, void *buf, uint64_t size)
{
    uint8_t *p = buf;
    uint64_t done = 0;
    struct file *filp;

    if (do_sys_llseek(fd, (loff_t)off, SEEK_SET) < 0) {
        return -1;
    }
    filp = get_file_by_fd(fd);
    if (filp == NULL) {
        return -1;
    }
    filp->f_pos = off;

    while (done < size) {
        ssize_t ret = do_sys_read(fd, p + done, size - done);
        if (ret <= 0) {
            return -1;
        }
        done += ret;
    }

    return 0;
}

static int exec_close(int fd)
{
    struct task_struct *task = get_current_task();
    struct file *filp;

    if (task == NULL || fd < 0 || fd >= MAX_FILE_NR) {
        return -1;
    }

    xos_spinlock(&task->files_set.file_lock);
    filp = task->files_set.file_set[fd];
    task->files_set.file_set[fd] = NULL;
    clear_bit(task->files_set.fd_map.bit_start, fd);
    xos_unspinlock(&task->files_set.file_lock);

    if (filp != NULL) {
        xos_kfree(filp);
    }
    return 0;
}

static int exec_reset_task_mm(struct task_struct *task, uint64_t *new_pgd_out)
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
    memset(task->mm, 0, sizeof(*task->mm));
    xos_spinlock_init(&task->mm->mm_lock);
    task->task_pgd = new_pgd;
    task->mm->mm_pgd = new_pgd;

    if (new_pgd_out != NULL) {
        *new_pgd_out = V2P(new_pgd);
    }
    return 0;
}

static int exec_build_user_layout(struct task_struct *task,
                                  uint64_t image_end)
{
    uint64_t stack_start = USER_STACK_START;
    uint64_t stack_end = USER_STACK_TOP;
    uint64_t heap_start = align_page_up(image_end);
    uint64_t heap_end = heap_start + XOS_EXEC_USER_HEAP_SIZE;
    int ret;

    task->mm->start_brk = heap_start;
    task->mm->end_brk = heap_end;

    ret = create_vma(task, stack_start, stack_end, XOS_EXEC_INV_PHY_ADDR,
                     VM_READ | VM_WRITE);
    if (ret < 0) {
        return ret;
    }

    ret = create_vma(task, heap_start, heap_end, XOS_EXEC_INV_PHY_ADDR,
                     VM_READ | VM_WRITE);
    if (ret < 0) {
        return ret;
    }

    ret = create_vma(task, (uint64)_TO_UVA_(UART_BASE),
                     (uint64)_TO_UVA_(UART_BASE) + PAGE_SIZE,
                     (uint64)UART_BASE, VM_READ | VM_WRITE | VM_SHARED);
    return ret;
}


/*
    2026.03.08 21:42

    load elf image
*/

static int exec_load_elf(struct task_struct *task, const char *pathname,
                               uint64_t *entry_out)
{
    int fd;
    int ret = -1;
    elf64_ehdr_t ehdr;
    elf64_phdr_t *phdrs = NULL;
    uint64_t i;
    uint64_t image_base = (uint64_t)-1;
    uint64_t image_end = 0;
    uint64_t load_size;
    uint64_t load_pages;
    int load_order;
    char *image_base_kva = NULL;
    uint64_t image_base_pa;
    uint64_t text_start = (uint64_t)-1;
    uint64_t text_end = (uint64_t)-1;
    uint64_t data_start = (uint64_t)-1;
    uint64_t data_end = (uint64_t)-1;
    int load_cnt = 0;
    int image_installed = 0;

    fd = do_sys_open((char *)pathname, O_RDONLY, 0);
    if (fd < 0) {
        return -1;
    }

    if (exec_read(fd, 0, &ehdr, sizeof(ehdr)) < 0) {
        goto out;
    }
    /* first check for the magic header. */
    if (ehdr.e_ident[0] != ELF64_MAGIC0 || ehdr.e_ident[1] != ELF64_MAGIC1 ||
        ehdr.e_ident[2] != ELF64_MAGIC2 || ehdr.e_ident[3] != ELF64_MAGIC3) {
        ret = -ENOEXEC;
        goto out;
    }
    if (ehdr.e_ident[4] != 2 || ehdr.e_ident[5] != 1) {
        ret = -ENOEXEC;
        goto out;
    }
    /* Check machine architecture field  and version*/
    if (ehdr.e_machine != 183 || ehdr.e_version != 1) {
        ret = -ENOEXEC;
        goto out;
    }
    /* Check file type */
    if (ehdr.e_type != ELF64_ET_EXEC) {
        ret = -ENOEXEC;
        goto out;
    }
    /* Check ELF header size */
    if (ehdr.e_ehsize != sizeof(elf64_ehdr_t)) {
        ret = -ENOEXEC;
        goto out;
    }
    /* Check prog header size */
    if (ehdr.e_phnum == 0 || ehdr.e_phentsize != sizeof(elf64_phdr_t)) {
        ret = -ENOEXEC;
        goto out;
    }

    /* Check number of prog headers */
    if (ehdr.e_phnum >= ELF_MAXPHNUM) {
        ret = -ENOEXEC;
        goto out;
    }
    phdrs = xos_kmalloc((uint64_t)ehdr.e_phnum * sizeof(elf64_phdr_t));
    if (phdrs == NULL) {
        ret = -ENOMEM;
        goto out;
    }

    if (exec_read(fd, ehdr.e_phoff, phdrs,
                        (uint64_t)ehdr.e_phnum * sizeof(elf64_phdr_t)) < 0) {
        goto out;
    }
    /*
        Calculate the hole image range
    */
    for (i = 0; i < ehdr.e_phnum; i++) {
        elf64_phdr_t *ph = &phdrs[i];

        if (ph->p_type != ELF64_PT_LOAD || ph->p_memsz == 0) {
            continue;
        }

        if (ph->p_vaddr < image_base) {
            image_base = align_page_down(ph->p_vaddr);
        }
        if (align_page_up(ph->p_vaddr + ph->p_memsz) > image_end) {
            image_end = align_page_up(ph->p_vaddr + ph->p_memsz);
        }
        load_cnt++;
    }

    if (load_cnt == 0 || image_base == (uint64_t)-1 || image_end <= image_base) {
        ret = -ENOEXEC;
        goto out;
    }
    /*
        2026.09.05
        calculate the order based on the image size and allocate memory space.
    */
    load_size = image_end - image_base;
    load_pages = (load_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
    load_order = 0;
    while ((1UL << load_order) < load_pages) {
        load_order++;
    }

    image_base_kva = (char *)xos_get_free_page(0, load_order);
    if (image_base_kva == NULL) {
        ret = -ENOMEM;
        goto out;
    }
    memset(image_base_kva, 0, PAGE_SIZE << load_order);
    image_base_pa = V2P(image_base_kva);
    /*
        Read ELF segment data in a loop and copy to target memory addresses
    */
    for (i = 0; i < ehdr.e_phnum; i++) {
        elf64_phdr_t *ph = &phdrs[i];

        if (ph->p_type != ELF64_PT_LOAD || ph->p_memsz == 0) {
            continue;
        }
        if (ph->p_filesz > ph->p_memsz) {
            ret = -ENOEXEC;
            goto out;
        }
        if (exec_read(fd, ph->p_offset,
                            image_base_kva + (ph->p_vaddr - image_base),
                            ph->p_filesz) < 0) {
            ret = -ENOEXEC;
            goto out;
        }
    }

    ret = exec_reset_task_mm(task, NULL);
    if (ret < 0) {
        goto out;
    }
    /*
        loop create segment VMA 
    */
    for (i = 0; i < ehdr.e_phnum; i++) {
        elf64_phdr_t *ph = &phdrs[i];
        uint64_t seg_start;
        uint64_t seg_end;
        uint64_t seg_pa;
        uint64_t seg_vma_flags;

        if (ph->p_type != ELF64_PT_LOAD || ph->p_memsz == 0) {
            continue;
        }

        seg_start = align_page_down(ph->p_vaddr);
        seg_end = align_page_up(ph->p_vaddr + ph->p_memsz);
        seg_pa = image_base_pa + (seg_start - image_base);

        if (ph->p_flags & ELF64_PF_W) {
            seg_vma_flags = VM_READ | VM_WRITE;
            if (ph->p_flags & ELF64_PF_X) {
                seg_vma_flags |= VM_EXEC;
            }
        } else {
            seg_vma_flags = VM_READ;
            if (ph->p_flags & ELF64_PF_X) {
                seg_vma_flags |= VM_EXEC | VM_SHARED;
            } else {
                seg_vma_flags |= VM_SHARED;
            }
        }

        ret = create_vma(task, seg_start, seg_end, seg_pa, seg_vma_flags);
        if (ret < 0) {
            goto out;
        }

        if (seg_vma_flags & VM_EXEC) {
            if (seg_start < text_start) {
                text_start = seg_start;
            }
            if (seg_end > text_end) {
                text_end = seg_end;
            }
        }
        if (seg_vma_flags & VM_WRITE) {
            if (seg_start < data_start) {
                data_start = seg_start;
            }
            if (seg_end > data_end) {
                data_end = seg_end;
            }
        }
    }

    task->mm->start_code = (text_start == (uint64_t)-1) ? image_base : text_start;
    task->mm->end_code =   (text_end == (uint64_t)-1)? text_end : image_end;
    task->mm->start_data = (data_start == (uint64_t)-1) ? task->mm->end_code : data_start;
    task->mm->end_data =   (data_end == (uint64_t)-1) ? data_end : task->mm->start_data;

    ret = exec_build_user_layout(task, image_end);
    if (ret < 0) {
        goto out;
    }

    vma_space_maps(task);
    set_ttbr0_el1((uint64)V2P(task->task_pgd));
    exec_flush_user_tlb();
    image_installed = 1;
    *entry_out = ehdr.e_entry;
    ret = 0;

out:
    if (fd >= 0) {
        exec_close(fd);
    }
    if (phdrs != NULL) {
        xos_kfree(phdrs);
    }
    if (ret < 0 && image_base_kva != NULL && !image_installed) {
        xos_free_page(image_base_kva);
    }
    return ret;
}

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

static char *exec_user_strdup(const char *src)
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
    if (copy_from_user(dst, src, len) != 0) {
            xos_kfree(dst);
            return NULL;
        }
    dst[len] = '\0';
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
    /*
        get arg count
    */
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
        dst[i] = exec_user_strdup(src[i]);
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
    int ret;

    if (pathname == NULL || regs == NULL) {
        return -1;
    }

    kpath = exec_user_strdup(pathname);
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
    /*
        2026.08.22  新增内置程序的判断和处理，不依赖文件系统
        应用程序直接和内核编译成同一镜像 另一个process_create()
    */
    if (exec_is_builtin_path(kpath)) {
        if (exec_rebuild_builtin_image(task) < 0) {
            exec_free_strv(kargv);
            exec_free_strv(kenvp);
            xos_kfree(kpath);
            return -1;
        }
        user_entry = get_user_entry();
    } else {
        ret = exec_load_elf(task, kpath, &user_entry);
        if (ret < 0) {
            exec_free_strv(kargv);
            exec_free_strv(kenvp);
            xos_kfree(kpath);
            return ret;
        }
    }
    task->user_entry = (task_fun)user_entry;
    strcpy(task->user_path, kpath);
    regs->pc = user_entry;
    regs->sp = USER_STACK_TOP;
    regs->pstate = PSR_MODE_EL0t;
    regs->regs[0] = 0;
    regs->regs[1] = 0;
    regs->regs[2] = 0;

    exec_free_strv(kargv);
    exec_free_strv(kenvp);
    xos_kfree(kpath);
    return 0;
}

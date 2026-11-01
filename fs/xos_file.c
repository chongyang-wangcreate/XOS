/********************************************************
    
    development started:2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 ~ 2028 wangchongyang

********************************************************/


#include "types.h"
#include "list.h"
#include "setup_map.h"
#include "string.h"
#include "tick_timer.h"
#include "bit_map.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_kern_def.h"
#include "fs.h"
#include "xos_cache.h"
#include "xos_file.h"
#include "task.h"
#include "error.h"

void xos_file_get(struct file *filp)
{
    if (filp == NULL) {
        return;
    }

    xos_spinlock(&filp->f_lock);
    filp->ref_count++;
    xos_unspinlock(&filp->f_lock);
}

void xos_file_put(struct file *filp)
{
    int comp_ret;

    if (filp == NULL) {
        return;
    }
    xos_spinlock(&filp->f_lock);
    if (filp->ref_count == 0) {
        xos_unspinlock(&filp->f_lock);
        return;
    }
    filp->ref_count--;
    comp_ret = filp->ref_count == 0;
    xos_unspinlock(&filp->f_lock);
    if (!comp_ret) {
        return;
    }

    if (filp->f_ops != NULL && filp->f_ops->release != NULL) {
        filp->f_ops->release(filp->f_dentry->file_node, filp);
    }
    if (filp->f_dentry != NULL) {
        xos_spinlock(&inode_lock);
        if (filp->f_dentry->file_node != NULL) {
            filp->f_dentry->file_node->ref_count--;
        }
        xos_unspinlock(&inode_lock);
    }
    xos_kfree(filp);
}

int do_sys_close(int fd)
{
    struct task_struct *task = current_task;
    struct file *filp;

    if (task == NULL || fd < 0 || fd >= MAX_FILE_NR) {
        return -EBADF;
    }
    xos_spinlock(&task->files_set.file_lock);
    filp = task->files_set.file_set[fd];
    if (filp != NULL) {
        task->files_set.file_set[fd] = NULL;
        clear_bit(task->files_set.fd_map.bit_start, fd);
    }
    xos_unspinlock(&task->files_set.file_lock);
    if (filp == NULL){
        return -EBADF;
    } 
    xos_file_put(filp);
    return 0;
}


/*
    need to bind with the current process

    Pay attention to concurrent operations
*/
int alloc_unused_fd(struct task_struct *tsk)
{
    int index;
    xos_spinlock(&(tsk->files_set.file_lock));
    index = find_free_bit(&tsk->files_set.fd_map);

    set_bit(tsk->files_set.fd_map.bit_start, index);
    xos_unspinlock(&tsk->files_set.file_lock);
    return index;

}

struct file *get_free_file(int fd)
{
   struct task_struct *tsk = current_task;

   if(fd < MAX_FILE_NR){
        return tsk->files_set.file_set[fd];
   }
   return NULL;
}


int file_fd_bind(struct file*new_file,int fd)
{
    struct task_struct *tsk = current_task;
       if(fd < MAX_FILE_NR){
        tsk->files_set.file_set[fd] = new_file;
        return 0;
    }
    return -1;
}

struct file * get_file_by_fd(int fd){
    struct file *file_node;
    struct task_struct *tsk = current_task;
    xos_spinlock(&tsk->files_set.file_lock);
    if(fd < MAX_FILE_NR){
        file_node = tsk->files_set.file_set[fd];
        xos_unspinlock(&tsk->files_set.file_lock);
        return file_node;
    }
    xos_unspinlock(&tsk->files_set.file_lock);
    return NULL;
}


loff_t generic_ram_file_llseek(struct file *filp, loff_t offset, int whence)
{
    if(whence != SEEK_CUR && whence != SEEK_SET && whence != SEEK_CUR){
        return -1; // tmp using error -1
    }
    
    xinode *tmp_node = filp->f_dentry->file_node;
    loff_t  tmp_pos;
    xos_spinlock(&tmp_node->lock);
    switch(whence){
        case SEEK_SET:
            if(offset > 0){
                tmp_pos = offset;
            }else{
                tmp_pos = 0;
            }
            tmp_node->pos = tmp_pos;
            break;

        case SEEK_CUR:
            if(offset < 0 && (-offset) > tmp_node->pos){
                offset = 0;
                tmp_node->pos = 0;
            }
            tmp_node->pos += offset;
            break;

        case SEEK_END:
            if(offset < 0 && (-offset) > tmp_node->pos){
                offset = 0;
                tmp_node->pos = 0;
            }
            tmp_node->pos += offset;
            break;
        default:
            break;
    }
    xos_unspinlock(&tmp_node->lock);
    return tmp_node->pos;
}


int do_sys_llseek(int fd,loff_t offset,int whence)
{
    int pos;
    struct file *filp = get_file_by_fd(fd);
    pos = filp->f_ops->llseek(filp,offset,whence);
    return pos;
}


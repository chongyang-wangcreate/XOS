#ifndef __SEM_H__
#define __SEM_H__

enum{
    SEM_BINARY,
    SEM_MUTEXT,
    SEM_COUNT
};
/*
    结构的定义非常关键，结构定义完成，相当于完成一半的工作量
*/
typedef struct sem_struct{
    xos_spinlock_t lock;//spinl_lock  保护临界资源
    int sem_types; //那种类型的信号量
    int init_count;
    int sem_count;//sem_val
    dlist_t wait_list_head;//wailt_list_head 将阻塞的进程加入其中,还需要增加等待队列

}xsem_t;

extern xsem_t  sem_val;

void sem_init(xsem_t *sem,int count,int type);
void sem_pend(xsem_t *);
void sem_post(xsem_t *);
    
#endif

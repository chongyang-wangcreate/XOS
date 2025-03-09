#ifndef __XOS_MUTEX_H__
#define __XOS_MUTEX_H__


typedef struct xos_mutex{
    xos_spinlock_t sp_lock;
    dlist_t       wait_list_head;
    int           wait_count;
    int           source;

}mutext_t;

extern void mutext_unlock(mutext_t *mutex);
extern void mutext_lock(mutext_t *mutex);
extern void mutext_init(mutext_t *mutex);



#endif

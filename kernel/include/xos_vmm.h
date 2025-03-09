#ifndef __XOS_MM_H__
#define __XOS_MM_H__



extern int create_vma(struct task_struct *t, unsigned long vm_start,
    unsigned long vm_end, unsigned long pma_saddr, unsigned long vm_flags);


#endif

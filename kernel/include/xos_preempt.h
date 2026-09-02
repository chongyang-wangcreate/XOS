#ifndef __XOS_PREEMPT_H__
#define __XOS_PREEMPT_H__

#include "task.h"

static inline u8 *preempt_count_ptr(void)
{
  struct task_struct *task = current_task;

  if(task == NULL){
    return NULL;
  }
  return &task->preempt_count;
}

static inline int preempt_count(void)
{
  u8 *count = preempt_count_ptr();

  if(count == NULL){
    return 1;
  }
  return *count;
}

static inline void preempt_count_add(int val)
{
  u8 *count = preempt_count_ptr();

  if(count != NULL){
    *count += val;
  }
}

static inline void preempt_count_dec(int val)
{
  u8 *count = preempt_count_ptr();

  if(count != NULL && *count >= val){
    *count -= val;
  }
}

 static inline int preempt_is_disabled(void) {
  return (preempt_count() > 0);
}


#define preempt_disable()\
do  \
{   \
    preempt_count_add(1);\
} while (0)

#define preempt_enable()\
do  \
{   \
    preempt_count_dec(1);\
} while (0)

#endif

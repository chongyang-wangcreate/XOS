#ifndef __XOS_PREEMPT_H__
#define __XOS_PREEMPT_H__


#define preempt_count()	(current_task_info_new()->preempt_count)

#define preempt_count_add(val) do { preempt_count() += (val); } while (0)
#define preempt_count_dec(val) do { preempt_count() -= (val); } while (0)

 static inline int preempt_is_disabled(void) {
  return (current_task_info_new()->preempt_count > 0);
}


#define preempt_disable()\
do  \
{   \
    current_task_info_new()->preempt_count++;\
} while (0)

#define preempt_enable()\
do  \
{   \
    current_task_info_new()->preempt_count--;\
} while (0)

#endif

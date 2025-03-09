#ifndef __WAIT_H__
#define __WAIT_H__

static inline void add_wait_queue(dlist_t *new_node,dlist_t *head)
{

    list_add_back(new_node, head);
}

static inline void del_from_queue(dlist_t *new_node)
{

    list_del(new_node);
}




#endif

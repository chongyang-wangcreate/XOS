#ifndef __LIST_H__
#define __LIST_H__

typedef struct list_head{
    struct list_head *next;
    struct list_head *prev;

}dlist_t;

#define offsetof(type, member) ((size_t) &((type *)0)->member)

#define list_entry(ptr,type,member) \
    (type*)((uint64)ptr - ((uint64)&((type*)0)->member))

#define list_for_each(ptr,head) for(ptr = head->next;ptr != head;ptr = ptr->next)

/*
static list_for_each(dlist_t *ptr,dlist_t *head)
{
    
}
*/

static inline void list_init(dlist_t *head)
{
    head->next = head;
    head->prev = head;
}
/*
    定义名称时起名 front 和after 自己把自己搞混乱了，自己应该重新改名
*/

static inline void list_add_front(dlist_t *new,dlist_t *head)
{
    new->next = head;
    new->prev = head->prev;
    new->next->prev = new;
    new->prev->next = new;
}

static inline void list_add_back(dlist_t *new,dlist_t *head)
{
    new->prev = head;
    new->next = head->next;
    new->prev->next = new;
    new->next->prev = new;
}

static inline int list_is_empty(dlist_t *head)
{
    if((head->next == head)||(head->next == NULL))
    {
        return 1;
    }
    return 0;
}


/*
    只获取链表头节点后面一个节点

    所以获取之后如果任务由运行态running ,重新变为ready 态，重新加入链表需要加到链表尾部，

    RR 轮询调度
*/
static inline dlist_t *list_get_first_node(dlist_t *head)
{
    if(head->next != head)
        return head->next;
    return NULL;
}
static inline void list_del(dlist_t *del_node)
{
    del_node->prev->next = del_node->next;
    del_node->next->prev = del_node->prev;
}

static inline void move_to_front(dlist_t *list, dlist_t *head)
{
    list_del(list);
    list_add_back(list, head);
}

#endif

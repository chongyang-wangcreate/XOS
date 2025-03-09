#ifndef __ULIST_H__
#define __ULIST_H__

typedef struct u_list_head{
    struct u_list_head *next;
    struct u_list_head *prev;

}u_dlist_t;

#define u_list_entry(ptr,type,member) \
    (type*)((uint64)ptr - ((uint64)&((type*)0)->member))

#define u_list_for_each(ptr,head) for(ptr = head->next;ptr != head;ptr = ptr->next)

/*
static list_for_each(dlist_t *ptr,dlist_t *head)
{
    
}
*/
static inline void u_list_init(u_dlist_t *head)
{
    head->next = head;
    head->prev = head;
}
/*
    定义名称时起名 front 和after 自己把自己搞混乱了，自己应该重新改名
*/

static inline void u_list_add_front(u_dlist_t *new,u_dlist_t *head)
{
    new->next = head;
    new->prev = head->prev;
    new->next->prev = new;
    new->prev->next = new;
}

static inline void u_list_add_after(u_dlist_t *new,u_dlist_t *head)
{
    new->prev = head;
    new->next = head->next;
    new->prev->next = new;
    new->next->prev = new;
}

static inline int u_list_is_empty(u_dlist_t *head)
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
static inline u_dlist_t *list_get_first_node(u_dlist_t *head)
{
    if(head->next != head)
        return head->next;
    return NULL;
}
static inline void u_list_del(u_dlist_t *del_node)
{
    del_node->prev->next = del_node->next;
    del_node->next->prev = del_node->prev;
}
#endif


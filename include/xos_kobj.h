#ifndef __KOBJ_H__
#define __KOBJ_H__


struct kobj_st{
    struct kobj_st *parent; /*attach parent*/
    dlist_t at_parent_head; /*同步本节点挂接到父设备*/
    dlist_t children; /*子设备对象挂接的链表头*/

}kobj_t;


#endif

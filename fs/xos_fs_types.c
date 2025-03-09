/********************************************************
    
    development start: 2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

   Copyright (c) 2024 ~ 2027 wangchongyang

********************************************************/

#include "types.h"
#include "list.h"
#include "string.h"
#include "printk.h"
#include "bit_map.h"
#include "arch_irq.h"
#include "spinlock.h"
#include "xos_mutex.h"
#include "xos_vfs.h"
#include "error.h"

dlist_t g_fs_list;

int xfs_type_register(xvfs_file_t *fs_type)
{
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    dlist_t *ltmp_node;
    dlist_t *head = &g_fs_list;
    xvfs_file_t *fs_tmp_node;
    list_for_each(ltmp_node, head){
        fs_tmp_node = list_entry(ltmp_node, xvfs_file_t, vfs_list);
        if(strcmp(fs_tmp_node->fs_type,fs_type->fs_type) == 0){
            return -EDUP_REG; /*重复注册*/
        }
    }
    list_init(&fs_type->vfs_list);
    list_add_back(&fs_type->vfs_list, &g_fs_list);
    printk(PT_DEBUG,"%s:%d\n\r",__FUNCTION__,__LINE__);
    return 0;
   
}
void fs_list_init()
{
    list_init(&g_fs_list);
}
xvfs_file_t *find_match_system(const char *fs_name)
{
    dlist_t *ptr;
    xvfs_file_t *fs_sytem;
    list_for_each(ptr, (&g_fs_list)){
        fs_sytem= list_entry(ptr, xvfs_file_t, vfs_list);
        if(strcmp(fs_sytem->fs_type,fs_name) == 0){
            printk(PT_DEBUG,"%s:%d,fs_name=%s\n\r",__FUNCTION__,__LINE__,fs_sytem->fs_type);
            return fs_sytem;
        }
           
    }
    return NULL;
}

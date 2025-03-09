/********************************************************
    development start:2023.12
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

   Copyright (c) 2023 - 2027 wangchongyang

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

********************************************************/

#include "types.h"
#include "list.h"
#include "string.h"
#include "spinlock.h"
#include "setup_map.h"
#include "mmu.h"
#include "phy_mem.h"
#include "printk.h"
#include "gic-v3.h"
#include "tick_timer.h"
#include "xos_mutex.h"
#include "fs.h"
#include "task.h"
#include "cpu_desc.h"
#include "spinlock.h"

cpu_desc_t cpu_array[CPU_NR];

/*
    0 比较特殊
*/
char prio_table[128] = {

 0,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 4,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 5,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 4,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 6,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 4,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 5,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,
 4,0,1,0,2,0,1,0,
 3,0,1,0,2,0,1,0,


};

void cpu_desc_init()
{
    int num = CPU_NR;
    int prio_num = 0;
    int i = 0;
    memset(&cpu_array[i],0,sizeof(cpu_array[i]));
    for(; i < num;i++){
        
        for(;prio_num < PRIO_MAX;prio_num++){
            list_init(&cpu_array[i].runqueue[prio_num].run_list);
            list_init(&cpu_array[i].wait_que[prio_num].delay_list);
            list_init(&cpu_array[i].wait_que[prio_num].event_list);
            list_init(&cpu_array[i].wait_que[prio_num].wait_list);
            list_init(&cpu_array[i].timer_list_head);
            xos_spinlock_init(&cpu_array[i].lock);
            cpu_array[i].run_count[prio_num] = 0;
            cpu_array[i].bitmap_runque_start = 0;
        }
        
        cpu_array[i].run_bitmap.bit_start = (uint8*)&(cpu_array[i].bitmap_runque_start);
        cpu_array[i].run_bitmap.btmp_bytes_len = 32;//32个bit

        cpu_array[i].run_bitmap.bit_start = (uint8*)&(cpu_array[i].bitmap_waitque_start);
        cpu_array[i].run_bitmap.btmp_bytes_len = 32;//32个bit

    }
    

}



int _get_hp_task(uint64 map_val)
{
    return prio_table[map_val];
}
void find_hp_task(){
    int h_priority;
    tcb_map  prio_entity = {0};
    if(prio_entity.task_prio_map.prio_group_0 != 0){
        h_priority = _get_hp_task(prio_entity.task_prio_map.prio_group_0);
    }else if(prio_entity.task_prio_map.prio_group_1 != 0){
        h_priority = _get_hp_task(prio_entity.task_prio_map.prio_group_1);
        h_priority += 64;
    }
}

void task_set_bit(uint64 task_prio_map, uint64 bit_index) {
//    uint64 index = bit_index / 64;
    uint64 offset = bit_index % 64;
    task_prio_map |= (1ULL << offset);
}

void task_clear_bit(uint64 task_prio_map, uint64 bit_index) {
//    uint64 index = bit_index / 64;
    uint64 offset = bit_index % 64;
    task_prio_map &= ~(1ULL << offset);
}

int task_is_bit_set(uint64 task_prio_map, uint64 bit_index) {
//    uint64 index = bit_index / 64;
    uint64 offset = bit_index % 64;
    return (task_prio_map & (1ULL << offset))!= 0;
}


void new_add_to_cpu_runqueue(struct task_struct *task){

    uint64 *prio_bitmap;
    u64 cpuid;
    cpuid = cur_cpuid();
    if(task->prio < 64){
            prio_bitmap = &cpu_array[cpuid].map_entity.task_prio_map.prio_group_0;
            task_set_bit(*prio_bitmap,task->prio);
            cpu_array[cpuid].map_entity.task_prio_map.group0_hit = 1;
        }else if(task->prio >= 64){
             prio_bitmap = &cpu_array[cpuid].map_entity.task_prio_map.prio_group_1;
             task_set_bit(*prio_bitmap,task->prio);
             cpu_array[cpuid].map_entity.task_prio_map.group1_hit = 1;
        }
    

}


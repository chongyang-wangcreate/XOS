/********************************************************
    
    development started:2025.1.21
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2025 - 2030 wangchongyang

    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

********************************************************/
#define MAX_PID 65535
#define PID_SEG_SIZE 64
#define PID_INIT_SIZE 32

/*
    0 --511

    512-1023

    1024-1535

    1536--2047
*/
#define pid_map_size (MAX_PID + 1)/PID_SEG_SIZE

typedef struct {
    int cur_pid;
    int next_pid;
    xos_spinlock_t pid_lock;
    uint64_t used_pids[pid_map_size];
} pid_desc;

pid_desc  pid_val;




void pid_set_bit(pid_desc* pd, uint64 bit_index) {
    int index = bit_index / PID_SEG_SIZE;
    int offset = bit_index % PID_SEG_SIZE;
    pd->used_pids[index] |= (1ULL << offset);
}

void pid_clear_bit(pid_desc* pd, uint64 bit_index) {
    int index = bit_index / PID_SEG_SIZE;
    int offset = bit_index % PID_SEG_SIZE;
    pd->used_pids[index] &= ~(1ULL << offset);
}

int pid_is_bit_set(pid_desc* pd, uint64 bit_index) {
    int index = bit_index / PID_SEG_SIZE;
    int offset = bit_index % PID_SEG_SIZE;
    return (pd->used_pids[index] & (1ULL << offset))!= 0;
}


int find_left_free_pid(pid_desc* pd, long pid_map_index) {
    int pid_index;
    int j;
    int low;
    int high;
    high = (pid_map_index >> PID_INIT_SIZE);
    low =  pid_map_index&(0xffffffff);
    if(low != 0xffffffff){
        for(j = 0; j < 32;j++){
            if(pd->used_pids[pid_map_index] &(1 << j)){
                continue;
            }
            break;
            if(j != PID_INIT_SIZE){
                return (pid_map_index*PID_SEG_SIZE +j);
            }
        }
    }else if(high != 0xffffffff){
            for(j = 0; j < PID_INIT_SIZE;j++){
            if(pd->used_pids[pid_map_index] &(1 << j)){
                continue;
            }
            break;
            if(j != PID_INIT_SIZE){
                return (pid_map_index*PID_SEG_SIZE +PID_INIT_SIZE+j);
            }
        }
    }

    
}


/*
    查找原则,从最低bit 开始查找
    找到之后，置位返回所在bit 位index

*/
int find_unset_bit(pid_desc* pd) {
    int low = 0;
    int high = MAX_PID;
    long i;
    int pid_index;
    for(i = 0;i < pid_map_size;i++){
        pid_index = find_left_free_pid(pd, i);
        if(pid_index < 0){
            continue;
        }
        return pid_index;
    }
    return -1;  // 未找到未置位的 bit 位
}



int alloc_pid()
{
    int pid;
    uint64 ret;
    if(pid_val.cur_pid > MAX_PID){
        /*
            find_free_pid()
        */
        ret = find_unset_bit(&pid_val);
        pid_set_bit(&pid_val, ret);
        return ret;
    }
    pid = pid_val.cur_pid;
    pid_val.cur_pid++;

    return pid;
}


void free_pid(int pid)
{
    if(pid_val.cur_pid == pid){

        pid_val.cur_pid--;
        pid_clear_bit(&pid_val,pid);
    }
    pid_clear_bit(&pid_val,pid);
}


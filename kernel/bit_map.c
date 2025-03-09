
/********************************************************
    
    development start:2024
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2024 ~ 2027 wangchongyang

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

********************************************************/

#include "bit_map.h"
#include "string.h"
#include "printk.h"

void bitmap_init(bitmap_t* btmp) {
   memset(btmp->bit_start, 0, btmp->btmp_bytes_len);   
}

/*
    我们使用一个Bit位代表一个buddy_section
*/

int find_free_bit(bitmap_t *btmap)
{
    uint32 i_byte;
    uint32 j;
    printk(PT_RUN,"%s:%d,btmap->btmp_bytes_len=%d\n\r",__FUNCTION__,__LINE__,btmap->btmp_bytes_len);
    for(i_byte = 0;i_byte < btmap->btmp_bytes_len;i_byte++){
        if(btmap->bit_start[i_byte] != 0xff){
            break;
        }
    }
    if(i_byte == btmap->btmp_bytes_len){
        printk(PT_RUN,"%s:%d,i_byte=%d\n\r",__FUNCTION__,__LINE__,i_byte);
        return -1;
    }
    for(j = 0; j < 8;j++){
        if(btmap->bit_start[i_byte] &(1 << j)){
            //not free bit
            continue;
        }
        break;
    }
    if(j == 8){
        printk(PT_RUN,"%s:%d\n\r",__FUNCTION__,__LINE__);
        return -1;
    }

    printk(PT_RUN,"%s:%d,i=%d,j=%d\n\r",__FUNCTION__,__LINE__,i_byte,j);
    return (i_byte*8+j);
    
}

int find_left_set_bit(bitmap_t *btmap)
{
    uint32 i_byte;
    uint32 j;
    
    for(i_byte = 0;i_byte < btmap->btmp_bytes_len;i_byte++){
        if(btmap->bit_start[i_byte] != 0x0){
            break;
        }
    }
    if(i_byte == btmap->btmp_bytes_len){
        return -1;
    }
    for(j = 0; j < 8;j++){
        if(btmap->bit_start[i_byte] &(1 << j)){
            //not free bit
            break;
        }
        
    }
    printk(PT_RUN,"i_byte=%d,j=%d\n\r",i_byte,j);
    return (i_byte*8+j);
}

void set_bit(uint8 *start,uint32 i)
{
/*
    uint8 *p = start;
    p += i >> 3;
    *p |= 1 << ((i%8));
*/
   uint8 *p = start;
   uint32_t byte_idx = i / 8;    
   uint32_t bit_odd  = i % 8;    
   p[byte_idx] |= (1 << bit_odd);
   printk(PT_RUN,"%s:%d JJJJJJJJJJbyte_idx=%d,bit_odd=%d\n\r",__FUNCTION__,__LINE__,byte_idx,bit_odd);

}

void clear_bit(void *start ,uint32 i)
{
/*
    uint8 *p = start;
    p += i >> 3;
    *p &= ~(1 << ( (i%8)));
*/
   uint8 *p = start;
   uint32_t byte_idx = i / 8;    
   uint32_t bit_odd  = i % 8;    
   p[byte_idx] &= ~(1 << bit_odd);
}







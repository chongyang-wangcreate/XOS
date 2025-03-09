#ifndef __BIT_MAP_H__
#define __BIT_MAP_H__

#include "types.h"
typedef struct bit_map {
   uint64 btmp_bytes_len;
   uint8* bit_start;
}bitmap_t;

#define PRIO_LOWEST  127


extern void bitmap_init(bitmap_t* btmp);
extern void set_bit(uint8 *start,uint32 i);
extern void clear_bit(void *start ,uint32 i);

extern int find_free_bit(bitmap_t *btmap);
extern int find_left_set_bit(bitmap_t *btmap);
#endif

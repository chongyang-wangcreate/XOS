#ifndef __UIO_H__
#define __UIO_H__

typedef struct xos_io {
  void *io_base; 
  size_t io_len; 
} xos_uio_t;

struct io_seg {
    unsigned int type;
    size_t io_offset;
    size_t seg_count;
    xos_uio_t *iov;
    unsigned long nr_segs;
    int idx;
    int start_idx;

};

extern unsigned long copy_from_user(void *to ,const void *from,unsigned long size);
extern unsigned long copy_to_user(void *to ,const void *from,unsigned long size);


#endif

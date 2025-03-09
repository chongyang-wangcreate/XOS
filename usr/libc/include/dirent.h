#ifndef __DIRENT_H__
#define __DIRENT_H__


typedef unsigned long size_t;
typedef struct dirent64 {
    u64             d_ino;
    long            d_off;
    unsigned short	d_reclen;
    unsigned char	d_type;
    char            d_name[0];
}dirent64_t;

#define BUF_SIZE  10240
#define IO_BLOCK_SIZE (BUF_SIZE / sizeof(dirent64_t))

typedef struct dirent_buf{
    dirent64_t *cur_dir_block;
    dirent64_t *next_dir_block;
    dirent64_t io_block[IO_BLOCK_SIZE];
}dir_slab;

/*
    当前未实现malloc 所以先使用数组代替
*/
typedef struct IO_dir {
    int io_fd;
    int dd_len;
    int dd_offset;
    char *dd_buf;
    void *dd_lock;
    size_t remain_bytes;
    dir_slab dd_slab;
    int flags;
}DIR;

#endif

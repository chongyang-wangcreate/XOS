
#include "types.h"
#include "usys.h"
#include "./libc/include/dirent.h"
#include "./libc/include/readdir.h"
#include "printf.h"

#define O_ACCMODE	00000003
#define O_RDONLY	00000000
#define O_WRONLY	00000001
#define O_RDWR		00000002

#define O_DIRECTORY	 040000	/* must be a directory */
#define O_NOFOLLOW	0100000	/* don't follow links */
#define O_DIRECT	0200000	/* direct disk access hint - currently ignored */
#define O_LARGEFILE	0400000

extern int open(const char *pathname, int flag,...);

#define MAX_DP_NR 128
static DIR dp_buf[MAX_DP_NR] = {0};

DIR * alloc_dp()
{
    int i = 0;
    for(;i < MAX_DP_NR;i++){
        if(dp_buf[i].flags == 0){
            dp_buf[i].flags = 1;
            return &dp_buf[i];
        }
    }
    return NULL;
}
DIR *fdgetdp(int fd)
{
    DIR *dp = alloc_dp();

    if (!dp)
        return NULL;

    dp->io_fd = fd;
    return dp;
}


DIR *opendir(const char *name)
{
    int fd;
    DIR *dp;

    fd = open(name, O_DIRECTORY | O_RDONLY);
    if (fd < 0)
    {
        return NULL;
    }

    dp = fdgetdp(fd);
    if (!dp) {
        
    }
    return dp;
}



dirent64_t *readdir(DIR *dirp){
    dirent64_t *cur_dent;
    int byte_read;
    int fd = dirp->io_fd;
    int size = sizeof(dirp->dd_slab.io_block);
    if (!dirp->remain_bytes){
        byte_read = SYS_CALL_DEF3(10,(uint64_t)fd, (uint64_t)dirp->dd_slab.io_block,(uint64_t)size);
        
        if(byte_read <= 0){
            return NULL;
        }
        dirp->remain_bytes = byte_read;
        dirp->dd_slab.next_dir_block =  dirp->dd_slab.io_block;
    }

    cur_dent = dirp->dd_slab.next_dir_block;
    dirp->dd_slab.cur_dir_block = dirp->dd_slab.next_dir_block;

    dirp->dd_slab.next_dir_block = (dirent64_t *)((char *)dirp->dd_slab.cur_dir_block + cur_dent->d_reclen);
    dirp->remain_bytes -= cur_dent->d_reclen;
    printf("%s:%d,dir->bytes_left=%d,dent->d_reclen=%d\n\r",__FUNCTION__,__LINE__,dirp->remain_bytes,cur_dent->d_reclen);
    printf("%s:%d,dent->d_name=%s\n\r",__FUNCTION__,__LINE__,cur_dent->d_name);
    return cur_dent;
}


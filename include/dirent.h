#ifndef _XOS_DIRENT_H
#define _XOS_DIRENT_H

#include "types.h"



struct xos_dirent64 {
    u64             d_ino;
    long            d_off;
    unsigned short	d_reclen;
    unsigned char	d_type;
    char            d_name[0];
};

#endif


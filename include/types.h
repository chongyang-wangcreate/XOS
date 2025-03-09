typedef unsigned int	uint;
typedef unsigned short	ushort;
typedef unsigned char	uchar;
typedef unsigned long	uint64;
typedef unsigned int	uint32;
typedef unsigned int u32;
typedef unsigned int U32;

typedef unsigned short	uint16;
typedef unsigned short  u16;
typedef unsigned char	uint8;
typedef unsigned char  u8;
typedef unsigned long u64;
typedef long int64_t;

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long uint64_t;
typedef int   int32_t;
typedef short int16_t;
//typedef char  int8_t;
typedef	unsigned long	uintptr_t;
typedef long ssize_t;

typedef int bool;
#ifndef NULL
#define NULL ((void*)0)
#endif


typedef int gid_t;
typedef int uid_t;
typedef int ino_t;
typedef int caddr_t;
typedef int dev_t;
typedef long off_t;
typedef long time_t;
typedef long clock_t;
typedef unsigned long blksize_t;
typedef unsigned long blkcnt_t;


typedef unsigned short nlink_t;
typedef unsigned long size_t;


/*
typedef int gid_t;
typedef int uid_t;
typedef int dev_t;
typedef int ino_t;
typedef int caddr_t;

typedef unsigned short nlink_t;

typedef unsigned long blksize_t;
typedef unsigned long blkcnt_t;

typedef long off_t;
typedef long time_t;
typedef long clock_t;
*/
typedef long ssize_t;

typedef unsigned long useconds_t;
typedef long suseconds_t;
typedef uint32_t mode_t;
typedef long long loff_t;

typedef unsigned short		umode_t;

#define ALIGN_UP(x, align)    (uint64)(((uint64)(x) +  (align - 1)) & ~(align - 1))
#define ALIGN_DOWN(x, align) ((uint64)(x) & ~((uint64)(align)-1))



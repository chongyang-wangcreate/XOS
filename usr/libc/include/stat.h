#ifndef __STAT_H__
#define __STAT_H__
struct timespce64{
	long tv_sec;
	long tv_nsec;
};
struct stat {
	int     st_dev;     /* ID of device containing file */
	ino_t     st_ino;     /* inode number */
	mode_t    st_mode;    /* protection */
	nlink_t   st_nlink;   /* number of hard links */
	uid_t     st_uid;     /* user ID of owner */
	gid_t     st_gid;     /* group ID of owner */
	dev_t     st_rdev;    /* device ID (if special file) */
	off_t     st_size;    /* total size, in bytes */
	blksize_t st_blksize; /* blocksize for file system I/O */
	blkcnt_t  st_blocks;  /* number of 512B blocks allocated */
	struct timespce64    st_atime;   /* time of last access */
	struct timespce64    st_mtime;   /* time of last modification */
	struct timespce64    st_ctime;   /* time of last status change */
};


#endif

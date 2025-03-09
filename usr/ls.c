/**********************************************************************************
     Development Started: 2025
     Copyright (C) 2024-2028 wangchongyang
     Email: rockywang599@gmail.com

    This program is licensed under the GPL v2 License. See LICENSE for more details.
************************************************************************************/


#include "types.h"
#include "usys.h"
#include "ustring.h"
#include "./libc/include/stat.h"
#include "printf.h"
#include "./libc/include/dirent.h"
#include "./libc/include/readdir.h"

extern int usr_strcmp(const char *p, const char *q);

#if 0
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
	time_t    st_atime;   /* time of last access */
	time_t    st_mtime;   /* time of last modification */
	time_t    st_ctime;   /* time of last status change */
};
#endif
void ls_parse_path(void *path_name)
{
//    int i;
//    struct stat st;

}

/*
    20250117 ：21：50
    pathname  路径名称存在问题
*/
int get_file_stat(const char * pathname, struct stat * statbuf)
{
    return  SYS_CALL_DEF2(9,(uint64_t)pathname, (uint64)statbuf);
}

static struct stat st;

void shell_ls_cmd(int argc,char *argv[]){

    int i;
    DIR  *dir;
    dirent64_t *dirent;
    printf("%s:%d\n\r",__FUNCTION__,__LINE__);
   
    if(argc == 1){
         dir = opendir("./");
         dirent = readdir(dir);
         get_file_stat(dirent->d_name,&st);
         printf("%s:%d,dirent->d_name=%s\n",__FUNCTION__,__LINE__,dirent->d_name);
    }else{
        for(i = 1; i < argc ;i++){
        /*
            先简化处理 ls pathname
        */
            dir = opendir(argv[i]);
            while((dirent = readdir(dir)) != NULL){
                printf("%s:%d,dirent->d_name=%s\n\r",__FUNCTION__,__LINE__,dirent->d_name);
                if(usr_strcmp(dirent->d_name,".") != 0 && usr_strcmp(dirent->d_name,"..") != 0){
                    
                    get_file_stat(dirent->d_name,&st);
                    printf("www%s:%d,dirent->d_name=%s,mode=%x\n\r",__FUNCTION__,__LINE__,dirent->d_name,st.st_mode);
                }
            }

        }
    }
    printf("%s:%d,mode=%lx\n",__FUNCTION__,__LINE__,st.st_mode);
    printf("%s:%d,st_blksize=%lx\n",__FUNCTION__,__LINE__,st.st_blksize);


}  


void poll_dir(const char *path_name)
{
    DIR *dp;
    dirent64_t *dirent;
    dp = opendir(path_name);

    while((dirent = readdir(dp)) != NULL){
        printf("%s:%d,dirent->d_name\n\r",__FUNCTION__,__LINE__,dirent->d_name);
    }
}

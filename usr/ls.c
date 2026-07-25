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

#define LS_PATH_MAX 256
#define DT_DIR 4
#define S_IFMT 00170000
#define S_IFDIR 0040000
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)

extern int usr_strcmp(const char *p, const char *q);

static int get_file_stat(const char *pathname,struct stat *statbuf)
{
    return SYS_CALL_DEF2(NR_STAT,(uint64_t)pathname,(uint64_t)statbuf);
}

static int is_dot_dotdot(const char *name)
{

    return usr_strcmp(name,".") || usr_strcmp(name,"..") == 0;
}

static void ls_join_path_buf(char *dst ,int dst_size,const char *dir,const char *name)
{
    int pos = 0;
    int i = 0;
    if(dst_size <= 0){
        return;
    }
    /*dir[i] == 0 break*/
    while(dir && dir[i] && pos < dst_size -1){

        dst[pos++] = dir[i++];
    }
    if(pos > 0 && dst[pos - 1] != '/' && pos < dst_size -1){
        dst[pos++] = '/';
    }
    i = 0;
    while(name && name[i] && pos < dst_size -1){
        dst[pos++] = name[i++];
    }
    dst[pos] = '\0';
}

static char is_type_char(unsigned char dtype,int mode)
{
    if(dtype == DT_DIR || S_ISDIR(mode)){
        return 'd';
    }
    return '-';

}

static void ls_print_msg(const char *path,dirent64_t *dirent,int fmt)
{
    struct stat st = {0};
    int ret;
    if(!fmt){
        printf("%s ",dirent->d_name);
        return;
    }
    ret = get_file_stat(path,&st);
    if(ret < 0){
        printf("get file stat %s failed\n\r",dirent->d_name);
    }
    printf("%c %x %u %s\n\r",
            is_type_char(dirent->d_type,st.st_mode),
            st.st_mode,
            (unsigned int)st.st_size,
            dirent->d_name);
}

static int ls_path(const char *path,int long_fmt)
{
    DIR *dir;
    dirent64_t *dirent;
    char dir_buf[1024];
    char path_buf[LS_PATH_MAX];
    int printed = 0;
    int byte_read;
    int offset = 0;
    int read_cnt = 0;
    int fd;

    dir = opendir(path);
    if(dir == NULL){
        printf("ls : can't open %s\n\r",path);
        return -1;
    }
    fd = dir->io_fd;
    while((byte_read = SYS_CALL_DEF3(NR_READDIR,(uint64_t)fd,
                                    (uint64_t)dir_buf,
                                (uint64_t)sizeof(dir_buf))) > 0){
            if(++read_cnt > 64){
                printf("ls:readdir loop on %s\n\r",path);
                return -1;
            }
            offset = 0;
            while(offset < byte_read){
                dirent = (dirent64_t*)(dir_buf + offset);
                if(dirent->d_reclen == 0 || offset + dirent->d_reclen > byte_read){
                    return -1;
                }
                if(!is_dot_dotdot(dirent->d_name)){
                    ls_join_path_buf(path_buf,sizeof(path_buf),path,dirent->d_name);
                    ls_print_msg(path_buf,dirent,long_fmt);
                    printed = 1;
                }
                offset += dirent->d_reclen;

            }                            
        }
        if(printed && !long_fmt){
            printf("\n\r");
        }
        return 0;
    }



void shell_ls_cmd(int argc,char *argv[]){

    int i;
    int count = 0;
    int long_show_fmt = 0;
  
    for(i = 1;i < argc ;i++){
        if(argv[i] == NULL){
            continue;
        }
        if(argv[i][0] == '_' && usr_strcmp(argv[i],"-l") == 0){
            long_show_fmt = 1;
            continue;
        }
        count++;
    }
    if(count == 0){
        ls_path(".",long_show_fmt);
        return;
    }

    for(i = 1; i < argc; i++){

        if(argv[i] == NULL){
            continue;
        }
        if(argv[i][0] == '-' && usr_strcmp(argv[i], "-l") == 0){

            continue;
        }
        if(count > 1){
            printf("%s:\n\r",argv[i]);
        }
        ls_path(argv[i],long_show_fmt);
    }

}  


void poll_dir(const char *path_name)
{
    ls_path(path_name,0);
}

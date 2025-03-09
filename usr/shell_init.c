/********************************************************
    
    development start: 2024.12.8
    All rights reserved
    author :wangchongyang

   Copyright (c) 2024 ~ 2034 wangchongyang

********************************************************/


//#include <stddef.h>
#include "types.h"
//#include "string.h"
#include "ulist.h"
//#include <unistd.h>
#include "stdlib.h"
#include "xos_fcntl.h"
#include "printf.h"
#include "shell.h"
#include "shell_cmd.h"


/*
    开发调试shell 遇到不少问题，累计耗时3天，当前实现了shell 基本功能，
    但是 shell 代码看着非常混乱，定义了多个变量和函数非常散乱，必须重构。当前要进一步封装
    打算再用一周时间完善shell 功能，实现字符删除，光标移动
*/
int handle_normal_char(char *input_c,int *input_pos,int *cur_len,char in_char_val);
void read_term_cmd(int fd);

#if 0
char* usr_strcpy(char* dst_, const char* src_) {
   char* r = dst_;		       // 用来返回目的字符串起始地址
   while((*dst_++ = *src_++));
   return r;
}



/*
    20250105 调试是list_init  ,list_entry 地址空间出现问题
    所以需要实现用户态的list

*/
#define MAX_ARG_NR 10

extern  int print_char(char c);
extern int open(const char *pathname, int flag,...);
int readline(int fd,char *stor_buf,int buf_size, int ctrl_mode);

int usr_strcmp(const char *p, const char *q)
{
    while(*p && *p == *q) {
        p++, q++;
    }
    if((*p == *q)&&(*p == '\0')){
        return 0;
    }
    return (uchar)*p - (uchar)*q;
}

void* user_memset(void *dst, int v, int n)
{
    uint8	*p;
    uint8	c;
    uint32	val;
    uint32	*p4;

    p   = dst;
    c   = v & 0xff;
    val = (c << 24) | (c << 16) | (c << 8) | c;

    // set bytes before whole uint32
    for (; (n > 0) && ((uint64)p % 4); n--, p++){
        *p = c;
    }

    // set memory 4 bytes a time
    p4 = (uint*)p;

    for (; n >= 4; n -= 4, p4++) {
        *p4 = val;
    }

    // set leftover one byte a time
    p = (uint8*)p4;

    for (; n > 0; n--, p++) {
        *p = c;
    }

    return dst;
}

int parse_insert_cmd(char *cmd_str,char *argv[],char space_token)
{
    char *cur_str;
    int argc = 0;
    int argv_idx = 0;
    if((cmd_str == NULL)|| (*cmd_str == '\0')){
        return -1;
    }
    /*
        Clean up dirty data
    */
   while(argv_idx < MAX_ARG_NR) {
      argv[argv_idx] = NULL;
      argv_idx++;
   }
    /*
        check cmd_str space
    */
    cur_str = cmd_str;
    while(*cur_str != '\0'){
        while(*cur_str == ' '){
            cur_str++;
        }
        if(*cur_str == '\0'){
            break;
        }
        argv[argc] = cur_str;

        /*
            split string
        */
        while(*cur_str != '\0' && *cur_str != space_token){
            cur_str++;
        }
        /*
            *cur_str == '\0'  大循环结束退出
            *cur_str == ' '   空格分隔符，设置子字符串结束
        */
        if(*cur_str == ' '){
            *cur_str = '\0';
            cur_str++;
        }

      if(*cur_str == '\r'){
            *cur_str = '\0';
        }
        if (argc > MAX_ARG_NR) {
            return -1;
        }
        argc++;
    }
    printf("XXXXXXXXXXXXXXXXXXXXXX  argc=%d\n\r",argc);
    return argc;
}

xos_sh_cmd *lookup_shell_cmd(int argc, char* argv[])
{
    u_dlist_t *shell_cmd_head = get_shell_cmd_head();
    u_dlist_t *list_cur;
    xos_sh_cmd *cur_shell;
    u_list_for_each(list_cur, shell_cmd_head){
        cur_shell = u_list_entry(list_cur, xos_sh_cmd, list_node);
        printf("XXXXXXXXXXXXXXXXXXXXXX  cmd_name =%s\n\r",cur_shell->cmd_name_str);
        printf("argv[0] =%s\n\r",argv[0]);
        if(usr_strcmp(cur_shell->cmd_name_str, argv[0]) == 0){
            return cur_shell;
        }
    }
    return NULL;
}
/*
    Do you need to register some commands first 
    This is more compact and standardized than calling directly
*/
void exec_cmd(int argc, char* argv[]){
    xos_sh_cmd *ret_shell;
    ret_shell = lookup_shell_cmd(argc, argv);
    printf("XXXXXXXXXXXXXXXXXXXXXX  cmd_name =%s\n\r",ret_shell->cmd_name_str);
    
    if(!ret_shell){
        return;
    }
    ret_shell->cmd_fun(argc,argv);
}


/*
    当前还未实现fork 和exec
    实现轻量级shell, 内部命令如ls  ,pwd,cat 等，会直接执行
    用户自定义命令，fore+exec

    20240101 ~~ 20240105
    当前接收字符串存在一些问题，为何接收不了'\n'
    当前先用特殊字符做命令结束符
    多个字符串分割
*/
static char* argv[MAX_ARG_NR] = {NULL};
static char cmd_buf[256];

#if 0
int shell_main(void)
{
    int fd;
    int argc;
    int len = 0;
    int ret;
    char tmp_char;
    int  input_pos = 0;
    int  cur_len = 0;
    ck_node_t *ck_ptr;
    /*
        调试中发现重大问题，input_c 数组开辟256 栈空间，会莫名其妙的出现
        用户态会访问内核态地址空间，而且异常地址是memset   改成128 就没问题
        开始怀疑栈空间不够更改后stack_vma 范围没有起作用这个问题一定要查清楚。
    */
    char input_c[128] = {0};

    xos_shell_cmd_init();
    fd = open("/dev/console",0, 0);
    while(1)
    {
/*
        print_char('H');
        print_char('E');
        print_char('L');
        print_char('L');
        print_char('O');
        print_char('W');
        print_char('O');
        print_char('R');
        print_char('L');
        print_char('D');
        print_char('\n');
        */
        /*
        len = read(fd,cmd_buf,12);
        cmd_buf[len-1] = '\0';
        printf("WWWWWWWWWWWWWWWW  len=%s\n\r",cmd_buf);
        */
        input_pos = 0;
        cur_len = 0;
        user_memset(input_c, 0x0, 128);
        while(1){
         len = read(fd,&tmp_char,1);
         printf("%s:%d,len=%d,tmp_char=%c\n\r",__FUNCTION__,__LINE__,len,tmp_char);
         if(len <= 0){
            user_memset(input_c, 0x0, 128);
            sleep(5);
         }else{
            /*
                check if ctrl key
            */
            ck_ptr = handle_special_char(tmp_char);
            if(ck_ptr != NULL){
                /*
                    not ctrl key
                */
                ret = ck_ptr->handler(input_c, &input_pos, &cur_len);
                if(ret == READ_CONT){
                    /*
                        某些特殊字符操作可能结束当前输入需要退出
                    */
                    continue;
                }else if(ret == READ_FIN){
                    break;
                }
            }else{
                    handle_normal_char(input_c,&input_pos,&cur_len,tmp_char);
                    if((tmp_char == '\r')||(tmp_char == '\n')){
                        input_c[input_pos -1] = '\0'; 
                        break;
                    }
            }


         }
     }
        usr_strcpy(cmd_buf, input_c);
//      readline(fd,cmd_buf,256, 0);
        printf("WWWWWWWWWWWWWWWW  cmd_buf=%s\n\r",cmd_buf);
        argc = parse_insert_cmd(cmd_buf,argv,' ');
        printf("HOOMER argv[0] =%s\n\r",argv[0]);
        exec_cmd(argc,argv);
        if(len){
            
        }
        /*
            int i;
            int *p_val = (int*)0x30000;
            for(i = 0; i < 102400;i++){
                *p_val = 0;
                p_val++;
            }
        */
//        sleep(50);
       
    }
    return 0;
}
#endif

int shell_main(void)
{
    int fd;
    int len = 0;
    char tmp_char;
    int fd0;
    int fd1;
    int shell_ret;
    /*
        调试中发现重大问题，input_c 数组开辟256 栈空间，会莫名其妙的出现
        用户态会访问内核态地址空间，而且异常地址是memset   改成128 就没问题
        开始怀疑栈空间不够更改后stack_vma 范围没有起作用这个问题一定要查清楚。
    */
    char input_c[128] = {0};

    xos_shell_cmd_init();
    fd = open("/dev/console",O_RDWR, 0);
    fd0 = dup(fd);
    fd1 = dup(fd);
    printf("%s:%d,fd0=%d,fd1=%d\n\r",__FUNCTION__,__LINE__,fd0,fd1);
    mkdir("/test",0);
    while(1)
    {
//        user_memset(input_c, 0x0, 128);
        printf("<XOS:>");
        while(1){
         len = read(fd,&tmp_char,1);
//       printf("%s:%d,len=%d,tmp_char=%c\n\r",__FUNCTION__,__LINE__,len,tmp_char);
         if(len <= 0){
            user_memset(input_c, 0x0, 128);
            sleep(5);
         }else{
            shell_ret = shell_handler_read_char(get_xos_shell(),tmp_char);
            if(shell_ret < 0){
                break;
            }

         }
     }

       
    }
    return 0;
}


void read_term_cmd(int fd)
{
    int argc;
    int len = 0;
    char tmp_char;
    int  input_pos = 0;
    int  cur_len = 0;
    char input_c[256] = {0};
    while(1){
         len = read(fd,&tmp_char,1);
         if(len <= 0){
            user_memset(input_c, 0x0, 256);
            sleep(5);
         }else{
            handle_normal_char(input_c,&input_pos,&cur_len,tmp_char);
            if((tmp_char == '\r')||(tmp_char == '\n')){
                input_c[input_pos -1] = '\0'; 
                break;
            }

         }
    }
    usr_strcpy(cmd_buf, input_c);
    argc = parse_insert_cmd(cmd_buf,argv,' ');
    exec_cmd(argc,argv);
    if(len){
        ;
    }
        
}


void get_char(int fd,char *cmd_buf)
{
    int len = 0;
    char tmp_char;
    int  input_pos = 0;
    int  cur_len = 0;
    char input_c[256] = {0};
    while(1){
         len = read(fd,&tmp_char,1);
         printf("%s:%d,len=%d\n\r",__FUNCTION__,__LINE__,len);
         if(len <= 0){
            user_memset(input_c, 0x0, 256);
            sleep(5);
         }else{
            handle_normal_char(input_c,&input_pos,&cur_len,tmp_char);
            if((tmp_char == '\r')||(tmp_char == '\n')){
                input_c[input_pos -1] = '\0'; 
                break;
            }

         }
    }
    usr_strcpy(cmd_buf, input_c);

}
void read_term_cmd(int fd)
{
    int argc;
    get_char(fd,cmd_buf);   
    argc = parse_insert_cmd(cmd_buf,argv,' ');
    exec_cmd(argc,argv);

}

int handle_normal_char(char *input_c,int *input_pos,int *cur_len,char in_char_val)
{
    int cur_pos;
    int tmp_len;
    cur_pos = *input_pos;
    tmp_len = *cur_len;
    input_c[cur_pos] = in_char_val;
    cur_pos++;
    tmp_len++;
    *cur_len = tmp_len;
    *input_pos = cur_pos;
    return 0;
}

/*

    ctrl_mode 控制实现回显

    如果每次处理单个字符的化，用户态需要循环读取字符，读取之后继续休眠，直到接收到结束符退出
    循环，无数据阻塞休眠，读取失败也要继续休眠

    开发readline 的时候问了ai 相关控制码 ascii 问题，确实非常方便
*/

int readline(int fd,char *stor_buf,int buf_size, int ctrl_mode){
    char tmp_char;
    int len;
    int  input_pos = 0;
    int  cur_len = 0;
    char input_c[256] = {0};
    while(1){
         len = read(fd,&tmp_char,1);
         printf("%s:%d,len=%d\n\r",__FUNCTION__,__LINE__,len);
         if(len <= 0){
            user_memset(stor_buf, 0x0, buf_size);
            sleep(5);
         }else{
            handle_normal_char(input_c,&input_pos,&cur_len,tmp_char);
            if((tmp_char == '\r')&&(tmp_char == '\n')){
                input_c[input_pos -1] = '\0'; 
                goto finish;
            }

         }


    }
finish:    
    return 0;
}


int org_readline(int fd,char *stor_buf,int buf_size, int ctrl_mode){
    int ret;
    int  len = 0;
    int  input_pos = 0;
    int  cur_len = 0;
    char tmp_char;
    ck_node_t *ck_block = NULL;
    char input_c[256] = {0};
    while(1){
         len = read(fd,&tmp_char,1);
         printf("%s:%d,len=%d\n\r",__FUNCTION__,__LINE__,len);
         if(len <= 0){
            user_memset(stor_buf, 0x0, buf_size);
            sleep(1);
         }else{
                
                ck_block = handle_special_char(tmp_char);
                if(ck_block){
                    ret = ck_block->handler(input_c, &input_pos, &cur_len);
                    if(ret < 0){
                        
                    }
                    
                }else
                {
                    
                     //   接收到'\r' 或者'\n' 本次输入结束，跳出循环
                    
                    printf("%s:%d\n\r",__FUNCTION__,__LINE__);
                    handle_normal_char(input_c,&input_pos,&cur_len,tmp_char);
                    if((tmp_char == '\r')||(tmp_char == '\n')){
                        input_c[input_pos -1] = '\0'; 
                        goto finish;
                    }
                }
            }
         }
   
finish:

    usr_strcpy(stor_buf,input_c);
    return 0;

}
#endif

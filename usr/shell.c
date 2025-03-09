/********************************************************
    
    development timer: 2025.1
    All rights reserved
    author :wangchongyang
    email:rockywang599@gmail.com

    Copyright (c) 2025 - 2028 wangchongyang

   
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

********************************************************/


#include "types.h"
#include "ulist.h"
#include "shell.h"
#include "shell_cmd.h"
#include "ustring.h"
#include "usys.h"
#include "ulist.h"
#include "stdlib.h"
#include "printf.h"
#include "xos_fcntl.h"
#include "printf.h"

/*
    The most important element in implementing this function is the cursor
    Comparison of cursor position and length

    The shell is currently implemented in a relatively rudimentary manner, 
    and further feature additions and optimizations will be made in the future.

*/
sh_desc_t  cmd_shell;
extern void* user_memset(void *dst, int v, int n);
extern int print_char(char c);
extern int open(const char *pathname, int flag,...);
int handle_normal_char(char *input_c,int *input_pos,int *cur_len,char in_char_val);
int shell_insert_char(sh_desc_t *shell,char in_char);
void shell_del_char(sh_desc_t *shell,int direction);



static char* argv[MAX_ARG_NR] = {NULL};
static char cmd_buf[256];

int parse_insert_cmd(char *cmd_str,char *argv[],char space_token)
{
    char *cur_str;
    int argc = 0;
    int argv_idx = 0;
    if((cmd_str == NULL)|| (*cmd_str == '\0')){
        return -1;
    }
    /*
        Clean up
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
    return argc;
}


/*
    tmp_read_term_cmd
    read_term_cmd
    handle_normal_char
    readline
    这几个接口废弃，这些shell接口实现的非常乱，重新按照描述对象的思想重新定义了shell 描述符
*/
void tmp_read_term_cmd(int fd)
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

int readline(int fd,char *stor_buf,int buf_size, int ctrl_mode){
    int ret;
    int  len = 0;
    int  input_pos = 0;
    int  cur_len = 0;
    char tmp_char;
    ck_node_t *ck_block = NULL;
    char input_c[256] = {0};
    while(1){
         len = read(fd,&tmp_char,1);
         if(len <= 0){
            user_memset(stor_buf, 0x0, buf_size);
            sleep(1);
         }else{
                
                ck_block = handle_special_char(tmp_char);
                if(ck_block){
                    ret = ck_block->handler(input_c, &input_pos, &cur_len);
                    if(ret < 0){
                        
                    }
                }
                else
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


/*
    The following are the interface functions currently in use
*/
xos_sh_cmd *lookup_shell_cmd(int argc, char* argv[])
{
    u_dlist_t *shell_cmd_head = get_shell_cmd_head();
    u_dlist_t *list_cur;
    xos_sh_cmd *cur_shell;
    u_list_for_each(list_cur, shell_cmd_head){
        cur_shell = u_list_entry(list_cur, xos_sh_cmd, list_node);
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
    
    if(!ret_shell){
        return;
    }
    ret_shell->cmd_fun(argc,argv);
}

int shell_main(void)
{
    int fd;
    int len = 0;
    char tmp_char;
//    int fd0;
//    int fd1;
    int shell_ret;
    /*
        调试中发现重大问题，input_c 数组开辟256 栈空间，会莫名其妙的出现
        用户态会访问内核态地址空间，而且异常地址是显示是memset   ,改成128 就没问题
        开始怀疑栈空间不够更改stack_vma 范围没有起作用这个问题后续一定要查清楚。
    */
    char input_c[128] = {0};

    xos_shell_cmd_init();
    mkdir("/home/test2", 0);
    mkdir("/test",0);
    fd = open("/dev/console",O_RDWR, 0);
//    fd0 = dup(fd);
//    fd1 = dup(fd);
//    printf("%s:%d,fd0=%d,fd1=%d\n\r",__FUNCTION__,__LINE__,fd0,fd1);
    while(1)
    {
        printf("<XOS:>");
        while(1){
         len = read(fd,&tmp_char,1);
         if(len <= 0){
            user_memset(input_c, 0x0, 128);
            sleep(1);
         }else{
            shell_ret = shell_handle_read_char(get_xos_shell(),tmp_char);
            if(shell_ret < 0){
                break;
            }

         }
     }

    }
    return 0;
}

void shell_write_char(sh_desc_t *shell,char in_char)
{
    print_char(in_char);
}



int shell_handle_read_char(sh_desc_t *shell,char in_char)
{
    int ret = 0;
    int kflags = 0;
    u_dlist_t *sh_tmp;
    u_dlist_t *sh_cmd_list = &shell->sh_cmd.sh_key_list_head;
    xos_sh_cmd *kcmd_shell;
//   printf("%s:%d,in_char=%d,in_char=%d\n\r",__FUNCTION__,__LINE__,in_char,(int)in_char);
    /*
        特殊字符直接处理
    */
    if((int)in_char == 27){
        kflags = 1;
        shell->sp_cmd.tmp_buf[shell->sp_cmd.global_idx] = in_char;
        shell->sp_cmd.global_idx++;
    }
    if(shell->sp_cmd.tmp_buf[0] == 27 &&((int)in_char == 91)){
        shell->sp_cmd.tmp_buf[shell->sp_cmd.global_idx] = in_char;
        return 0;
    }
    if(shell->sp_cmd.tmp_buf[0] == 27 &&shell->sp_cmd.tmp_buf[1] == 91){
        shell->sp_cmd.tmp_buf[0] = 0;
        shell->sp_cmd.tmp_buf[1] = 0;
        shell->sp_cmd.global_idx = 0;
        u_list_for_each(sh_tmp, sh_cmd_list){
        kcmd_shell = u_list_entry(sh_tmp, xos_sh_cmd, list_node);
        if(kcmd_shell->ascii_code == (int)in_char){
                ret = kcmd_shell->kcmd_fun(shell);
                return ret;
            }
        }
        return 0;
    }
    u_list_for_each(sh_tmp, sh_cmd_list){
    kcmd_shell = u_list_entry(sh_tmp, xos_sh_cmd, list_node);
        if(kcmd_shell->ascii_code == (int)in_char){
            ret = kcmd_shell->kcmd_fun(shell);
            kflags = 1;
            if(kcmd_shell->ascii_code == 13){
                
                //printf("\n");
            }
//          printf("%s:%d,ret=%d\n\r",__FUNCTION__,__LINE__,ret);
            return ret;
        }
    }
    /*
        普通字符加入队列
    */
    if(kflags == 0){
        
        ret = shell_insert_char(shell,in_char);
        if(ret < 0){
            return INSER_END;
        }

    }
    return 0;
}

/*
    重写优化了shell 相关操作，原来实现比较分散混乱，现在统一整合
    定义成sh_desc_t
*/
int shell_insert_char(sh_desc_t *shell,char in_char)
{
    int i;

    if(shell->parse_cmd.cur_length > shell->parse_cmd.input_buf_size -1){
        printf("insert cmd to long\n");
        return -1;
    }
    int cur_cursor = shell->parse_cmd.cur_cursor;
    if(cur_cursor == shell->parse_cmd.cur_length){
        
        shell->parse_cmd.input_buf[cur_cursor] = in_char;
        shell->parse_cmd.cur_length++;
        cur_cursor++;
        shell->parse_cmd.cur_cursor = cur_cursor;
        shell_write_char(shell, in_char);

    }else if(cur_cursor < shell->parse_cmd.cur_length){

        for (i = (shell->parse_cmd.cur_length - shell->parse_cmd.cur_cursor); i > 0; i--)
        {
            shell->parse_cmd.input_buf[shell->parse_cmd.cur_cursor + i] =
                shell->parse_cmd.input_buf[shell->parse_cmd.cur_cursor + i - 1];
        }
        shell->parse_cmd.input_buf[shell->parse_cmd.cur_cursor] = in_char;
        shell->parse_cmd.cur_length++;
        for ( i = shell->parse_cmd.cur_cursor; i < shell->parse_cmd.cur_length; i++)
        {
            shell_write_char(shell, shell->parse_cmd.input_buf[i]);
        }
        shell->parse_cmd.cur_cursor++;
        shell->parse_cmd.input_buf[shell->parse_cmd.cur_length] = '\0';
        /*
            回退与光标位置一致
        */
        for ( i = shell->parse_cmd.cur_length - shell->parse_cmd.cur_cursor; i > 0; i--)
        {
            shell_write_char(shell, '\b');
        }
       
        return 0;
    }
    return 0;
}


/*
    direction 是后来添加，删除向前删除，向后删除
    0 :向前删除
    1 :向后删除
    调试中发现不少问题，尤其是光标位置回退有问题，有时没回退
    边写边测试能尽快发现自身问题
    2025.2.12 完善了shell，处理了一个Bug
*/

void shell_del_char(sh_desc_t *shell,int direction)
{
    int cur_cursor = shell->parse_cmd.cur_cursor;
    int cur_len = shell->parse_cmd.cur_length;
    int i;
    /*
        如果当前光标等于长度，输入向后删除，则返回，不做动作
    */

    if(cur_cursor == cur_len){
        if(direction == 1){
            /*
                prompt message  
            */
        }else if(direction == 0){
            /*
                改变光标位置，改变长度，减小cur_cursor   --,   cur_lenght--;
            */
            if(shell->parse_cmd.cur_cursor <= 0){
                return;
            }
            cur_cursor--;
            cur_len--;
            shell->parse_cmd.cur_cursor = cur_cursor;
            shell->parse_cmd.cur_length = cur_len;
            shell->parse_cmd.input_buf[cur_cursor] = '\0';//当前光标位置清0
            shell_write_char(shell,'\b'); /*退格*/
            shell_write_char(shell,' '); /*清成空格*/
            shell_write_char(shell,'\b');/*退格*/

        }

    }else if((cur_cursor < cur_len)&&(cur_cursor >= 0)){


        if(shell->parse_cmd.cur_cursor <= 0 && (direction == 0)){
             return;
        }
        if(direction == 0){
            i = 0;
            while(i < shell->parse_cmd.cur_length - shell->parse_cmd.cur_cursor){
                
            shell->parse_cmd.input_buf[shell->parse_cmd.cur_cursor + i - 1] =
                shell->parse_cmd.input_buf[shell->parse_cmd.cur_cursor + i];
                i++;
            }
            shell->parse_cmd.cur_cursor--; /*光标位置减1*/
            shell_write_char(shell, '\b');/*光标左移*/
        }else{
            i = 1;
            while(i < shell->parse_cmd.cur_length - shell->parse_cmd.cur_cursor){
                
            shell->parse_cmd.input_buf[shell->parse_cmd.cur_cursor + i - 1] =
                shell->parse_cmd.input_buf[shell->parse_cmd.cur_cursor + i];
                i++;
            }
        }

        shell->parse_cmd.cur_length--;
        shell->parse_cmd.input_buf[shell->parse_cmd.cur_length] = 0;

        i = shell->parse_cmd.cur_cursor;   /*输出光标后的字符*/
        while(i < shell->parse_cmd.cur_length){
            shell_write_char(shell, shell->parse_cmd.input_buf[i]);/*光标后移*/
            i++;
        }

        shell_write_char(shell, ' ');
        /*光标回退*/
        for (i= 0;i < shell->parse_cmd.cur_length - shell->parse_cmd.cur_cursor + 1;  i++)
        {
            shell_write_char(shell, '\b');
        }


  }
}



/*
    refactoring   parse_insert_cmd
*/
void parse_input_cmd(sh_desc_t *shell)
{
    int  section_mark = 1;
    int i = 0; 
    int start_cursor = 0;
    
    shell->parse_cmd.argc = 0;
    for (short i = 0; i < MAX_ARG_NR; i++)
    {
        shell->parse_cmd.argv[i] = NULL;
    }
    while(shell->parse_cmd.input_buf[i] == ' '){  /*filter spaces*/
        i++;
        start_cursor++;
    }
    for (i = start_cursor; i < shell->parse_cmd.cur_length; i++)
    {
        if ( (shell->parse_cmd.input_buf[i] != ' '
                && shell->parse_cmd.input_buf[i] != '\0')) /*过滤有效字符串*/
        {

            if (section_mark == 1)
            {
                if (shell->parse_cmd.argc < MAX_ARG_NR)
                {
                    shell->parse_cmd.argv[shell->parse_cmd.argc] = &(shell->parse_cmd.input_buf[i]);
                    shell->parse_cmd.argc++;
                }
                section_mark = 0;
            }
            if (shell->parse_cmd.input_buf[i + 1] != '0')
            {
                i++;
            }
        }
        else
        {
            /*
                字符串分割,存在空格置为分割标志位
            */
            shell->parse_cmd.input_buf[i] = '\0';
            section_mark = 1;
        }
    }
}


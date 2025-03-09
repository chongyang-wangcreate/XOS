#ifndef __SHELL_H__
#define __SHELL_H__

#define MAX_ARG_NR 10
#define MAX_BUF_SIZE 128

enum{
    DEL_FRONT,
    DEL_AFTER,

};


typedef struct shell_history{
    
    char *history[MAX_ARG_NR];

}sh_his_msg;
typedef struct shell_cmd_parse{
    int key_val; /*shell 命令字符*/
    int cur_length; //当前长度
    int cur_cursor; //当前光标位置
    char *argv[MAX_ARG_NR]; //支持最大输入参数分量
    char input_buf[MAX_BUF_SIZE];
    int  input_buf_size;
    int  argc;

}sh_parse;

typedef struct shell_cmd{
    u_dlist_t  sh_key_list_head;
    u_dlist_t  sh_cmd_list_head;

}shell_cmd_t;

typedef struct special_filter{

    int global_idx;
    int tmp_buf[3];
}sp_filter;
typedef struct shell_ops{

    int (*sh_read)(int fd , char *buf, int size);
    int (*sh_write)(int fd , char *buf, int size);

}sh_ops;
typedef struct shell_desc{
    sh_parse    parse_cmd;
    sh_his_msg  history;
    sh_ops      shell_ops;
    shell_cmd_t sh_cmd;
    sp_filter   sp_cmd;
}sh_desc_t;


extern int backspace_key_handle(sh_desc_t *shell);
extern int delete_key_handle(sh_desc_t *shell);
extern int enter_key_handle(sh_desc_t *shell);
extern void exec_cmd(int argc, char* argv[]);

extern int shell_handle_read_char(sh_desc_t *shell,char in_char);
extern void shell_write_char(sh_desc_t *shell,char in_char);
extern int left_key_handle_cursor(sh_desc_t *shell);
extern int right_key_handle_cursor(sh_desc_t *shell);
extern void shell_del_char(sh_desc_t *shell,int direction);
extern void parse_input_cmd(sh_desc_t *shell);



#endif

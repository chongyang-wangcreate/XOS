#ifndef __SHELL_CMD_H
#define __SHELL_CMD_H

#define CTRL_K(x) ((x) - '@')  // 控制字符
enum{
    INSER_NORMAL,
    INSER_END

};

/*

    The important thing is to define the cmd structure
*/
typedef int (*sh_cmd_fun)(int argc, char*argv[]);
typedef int(*ksh_cmd_fun)(sh_desc_t *shell);
typedef struct xos_shell_cmd{
    union {
        char *cmd_name_str;  // 对于xos_shell_cmd，存储命令名称字符串
        int ascii_code;      // 对于xos_shell_kcmd，存储ASCII码
    };
    char *cmd_help_str;
    char *cmd_format;
    char *cmd_fun_desc;
    union{
        sh_cmd_fun cmd_fun;
        ksh_cmd_fun kcmd_fun;
    };
    u_dlist_t list_node;
    int alloc_flags;

}xos_sh_cmd;




typedef int (*ckey_fun)(char *buf, int *cur_pos, int *len);

typedef struct ckey_hash_node {
    int key;                    // 控制字符（比如 1 表示 Ctrl + A）
    ckey_fun handler;    // 对应的处理函数
    int alloc_flags;
    struct ckey_hash_node *next;
} ck_node_t;



extern sh_desc_t *get_xos_shell();

extern void xos_shell_cmd_init();

extern u_dlist_t *get_shell_cmd_head();

extern ck_node_t* handle_special_char(char ctrl_char);
extern void shell_ls_cmd(int argc,char **argv);



#endif

/*
     Development Started: 2024
     Copyright (C) 2024-2027 wangchongyang
     Email: rockywang599@gmail.com

    This program is licensed under the GPL v2 License. See LICENSE for more details.
*/

#include "types.h"
#include "ulist.h"
#include "shell.h"
#include "shell_cmd.h"
#include "ustring.h"
#include "usys.h"
#include "printf.h"

#define HASH_TABLE_SIZE 128


static sh_desc_t  xos_shell;

static xos_sh_cmd sh_cmd_block[HASH_TABLE_SIZE];


static char *shell_logo[] =
{
    " welcom to XOS (implemented by wangchongyang)  ",
    "-----------------------------------------------",
    "     *       *   * *  *          ***           ",
    "      *     *    *    *         *   *          ",
    "       *   *     *    *         *              ",
    "        * *      *    *          *             ",
    "         *       *    *      * * *             ",
    "        * *      *    *     *                  ",
    "       *   *     * *  *      *                 ",
    "      *     *             * * *                ",
    "-----------------------------------------------",
    NULL,
};




static void show_os_logo(void)
{
    int line_index	= 0;

    while (shell_logo[line_index])
    {
        printf("\t\t\t\t%s\n", shell_logo[line_index]);
        line_index++;
    }
    return;
}

sh_desc_t *get_xos_shell()
{
    return &xos_shell;
}

/*
    开始用了hash ，本来是正确了，特殊字符处理存在问题，所以注册部分又
    进行了重写
*/
static ck_node_t ctrl_ascii_buf[HASH_TABLE_SIZE] = {0};

unsigned int hash(int key) {
    return key % HASH_TABLE_SIZE;
}


void hash_register_handler(int ctrl_char, ckey_fun handler) {
    unsigned int index = hash(ctrl_char);
    ck_node_t* newNode = &ctrl_ascii_buf[index];
    newNode->key = ctrl_char;
    newNode->handler = handler;
}

void register_handler(int ctrl_char, ckey_fun handler) {
    int i;
    for(i = 0; i < 128;i++){
        if(ctrl_ascii_buf[i].alloc_flags == 0){
            break;
        }
    }
    if(i < 128){
        ctrl_ascii_buf[i].alloc_flags = 1;
        ck_node_t* newNode = &ctrl_ascii_buf[i];
        newNode->key = ctrl_char;
        newNode->handler = handler;
    }

}


ck_node_t* hash_handle_special_char(char ctrl_char) 
{
    int local_char; 
    local_char = CTRL_K(ctrl_char);
    unsigned int index = hash(local_char);
    printf("%s:%d,index=%d\n\r",__FUNCTION__,__LINE__,index);
    ck_node_t* current = &ctrl_ascii_buf[index];

    if (current != NULL) {
        if (current->key == ctrl_char) {
            return current;
        }
        return NULL;
    }
    return NULL;
}

ck_node_t* handle_special_char(char ctrl_char) 
{
    int i;
    int local_char; 
    local_char = CTRL_K(ctrl_char);
    unsigned int index = hash(local_char);
    printf("%s:%d,index=%d\n\r",__FUNCTION__,__LINE__,index);
    for(i = 0; i < 128;i++){
        if(ctrl_ascii_buf[i].key == index){
            return &ctrl_ascii_buf[i];
        }
    }
    return NULL;
}


u_dlist_t *get_shell_cmd_head()
{
    return &xos_shell.sh_cmd.sh_cmd_list_head;
}

void shell_cmd_head_init()
{
    int i = 0;
    u_list_init(&xos_shell.sh_cmd.sh_key_list_head);
    u_list_init(&xos_shell.sh_cmd.sh_cmd_list_head);
    for(i = 0; i < sizeof(xos_shell.parse_cmd.input_buf);i++){
        xos_shell.parse_cmd.input_buf[i] = '\0';
    }
    
    xos_shell.parse_cmd.input_buf_size = sizeof(xos_shell.parse_cmd.input_buf);
}

int handle_ctrl_a(char *buf, int *cur_pos, int *len)
{

    return INSER_NORMAL;
}

int handle_ctrl_b(char *buf, int *cur_pos, int *len)
{
    /*
        Ctrl + B（ASCII值 2，十六进制为 0x02）通常被称为水平制表符（Backspace），
        在一些文本模式下用于将光标向左移动一个位置，但它不执行字符删除的操作
    */
    
    int this_cursor = *cur_pos;

    if (this_cursor) 
    {
        printf("%c", '\b');
        this_cursor--;
    }

    *cur_pos = this_cursor;
    
     return INSER_NORMAL;
}

int handle_ctrl_c(char *buf, int *cur_pos, int *len)
{
    printf("%s:%d\n\r",__FUNCTION__,__LINE__);
    printf("%c", '\a');
    return INSER_END;
}

int handle_ctrl_d(char *buf, int *cur_pos, int *len)
{
    return INSER_NORMAL;
}

int handle_ctrl_h(char *buf, int *cur_pos, int *len)
{
    /*
        Ctrl + H（ASCII值 8，十六进制为 0x08）通常被称为退格键（Backspace），
        用于删除光标之前的一个字符。在文本编辑环境中，按下退格键时，
        光标左侧的字符会被删除。
    */
    return INSER_NORMAL;
}



void shell_ctrl_key_init()
{
    register_handler(CTRL_K('A'), handle_ctrl_a);
    register_handler(CTRL_K('B'), handle_ctrl_b);
    register_handler(CTRL_K('C'), handle_ctrl_c);
    register_handler(CTRL_K('D'), handle_ctrl_d);
    register_handler(CTRL_K('H'), handle_ctrl_h);
}

void shell_cmd_init(xos_sh_cmd  *new_cmd)
{
    u_list_init(&new_cmd->list_node);
}
xos_sh_cmd *alloc_cmd_cache()
{
    int i;
    for(i = 0;i < 256;i++){
         if(sh_cmd_block[i].alloc_flags == 0){
            sh_cmd_block[i].alloc_flags = 1;
            shell_cmd_init(&sh_cmd_block[i]);
            return &sh_cmd_block[i];
         }
    }
   return NULL;
}
int xos_shell_cmd_register(char *cmd_name,sh_cmd_fun cmd_fun,char *cmd_format,char *help,char *cmd_fun_desc)
{
    if((cmd_name == NULL)||(cmd_format == NULL)||(help == NULL)||(cmd_fun_desc == NULL)){
        return -1;
    }
    xos_sh_cmd  *new_cmd = NULL;
    new_cmd = alloc_cmd_cache();
    if(!new_cmd){
        return -1;
    }
    
    new_cmd->cmd_name_str = cmd_name;
    new_cmd->cmd_fun = cmd_fun;
    new_cmd->cmd_help_str = help;
    new_cmd->cmd_format = cmd_format;
    new_cmd->cmd_fun_desc = cmd_fun_desc;
    u_list_add_after(&new_cmd->list_node, &xos_shell.sh_cmd.sh_cmd_list_head);
    return 0;
}


int xos_shell_kmd_register(int keynum,ksh_cmd_fun cmd_fun,char *cmd_format,char *help,char *cmd_fun_desc)
{

    xos_sh_cmd  *new_cmd = NULL;
    new_cmd = alloc_cmd_cache();
    if(!new_cmd){
        return -1;
    }
    
    new_cmd->ascii_code = keynum;
    new_cmd->kcmd_fun = cmd_fun;
    new_cmd->cmd_help_str = help;
    new_cmd->cmd_format = cmd_format;
    new_cmd->cmd_fun_desc = cmd_fun_desc;
    u_list_add_after(&new_cmd->list_node, &xos_shell.sh_cmd.sh_key_list_head);
    return 0;
}


int test_shell_cmd(int argc, char*argv[]){
    /*
        check parampter
    */
    return 0;
}


int cat_cmd(int argc, char*argv[]){
    /*
        check parampter
    */
    if(argc != 2){
        return -1;
    }
    return 0;
}


int pwd_cmd(int argc, char*argv[]){
    char pwd_buf[256];
    /*
        check parampter
    */
    if(argc != 1){
        return -1;
    }

    SYS_CALL_DEF2(NR_PWD,(uint64_t)pwd_buf, (uint64_t)256);
    return 0;
}

int cd_cmd(int argc, char*argv[]){

    if(argc != 2){
        return -1;
    }

    SYS_CALL_DEF1(6,(uint64_t)argv[1]);
    return 0;
}

int ls_cmd(int argc, char*argv[]){
    printf("cd_cmd cd_cmd cd_cmd%s:%d\n\r",__FUNCTION__,__LINE__);
    if(argc == 2)
    {
        shell_ls_cmd(argc,argv);
    }else if((argc == 2)&&(argv[1][0] == '-')){
        if(!usr_strcmp(argv[1],"-l"))
        {
            /*to do*/
        }
    }

    return 0;
}

int load_exec_cmd(int argc, char*argv[]){

    printf("%s:%d\n\r",__FUNCTION__,__LINE__);
    return 0;
}

int backspace_key_handle(sh_desc_t *shell)
{
    shell_del_char(shell,DEL_FRONT);
    return 0;
}

int delete_key_handle(sh_desc_t *shell)
{
    shell_del_char(shell,DEL_AFTER);
    return 0;
}

void reset_shell_param(sh_desc_t *shell)
{
    int argv_idx = 0;
    int buf_size = sizeof(shell->parse_cmd.input_buf);
    user_memset(shell->parse_cmd.input_buf, 0x0, buf_size);
    shell->parse_cmd.cur_cursor = 0;
    shell->parse_cmd.cur_length = 0;

   while(argv_idx < MAX_ARG_NR) {
        shell->parse_cmd.argv[argv_idx] = NULL;
        argv_idx++;
   }
    
}
/*
    20250303:22:20
    以前版本没有任何问题，当前版本直接敲击回车按键
    内核态会出现异常，异常地址是打印函数
    这个问题增加如下语句暂时规避一下，具体问题后续再处理
    if(shell->parse_cmd.argv[0] == NULL){
        return -1;
    }
    本地版本管理不容易追踪问题
*/
int enter_key_handle(sh_desc_t *shell)
{

    printf("\n");
    parse_input_cmd(shell);
    if(shell->parse_cmd.argv[0] == NULL){
        return -1;
    }
    exec_cmd(shell->parse_cmd.argc,shell->parse_cmd.argv);
    reset_shell_param(shell);
    return -1;
}

int left_key_handle_cursor(sh_desc_t *shell)
{
    if(shell->parse_cmd.cur_cursor > 0){
        
        shell->parse_cmd.cur_cursor--;
        shell_write_char(shell,'\b');
    }
    return 0;
}

int right_key_handle_cursor(sh_desc_t *shell)
{
    int cur_cursor;
//  printf("%s:%d,%c\n",__FUNCTION__,__LINE__,shell->parse_cmd.input_buf[shell->parse_cmd.cur_cursor]);
    if(shell->parse_cmd.cur_cursor < shell->parse_cmd.cur_length){
        cur_cursor = shell->parse_cmd.cur_cursor;
        shell_write_char(shell,shell->parse_cmd.input_buf[cur_cursor]);
        shell->parse_cmd.cur_cursor++;
    }
    return 0;
}


/*
    shell 功能还存在问题，还需要处理某些特殊字符的bug
*/

void spcial_key_cmd_init()
{
    xos_shell_kmd_register(127, backspace_key_handle,   NULL,NULL,"backspace");
    xos_shell_kmd_register(126, delete_key_handle,      NULL,NULL,"delete");
    xos_shell_kmd_register(13,  enter_key_handle,       NULL,NULL,"enter");
    xos_shell_kmd_register(68,  left_key_handle_cursor, NULL,NULL,"left");  
    xos_shell_kmd_register(67,  right_key_handle_cursor,NULL,NULL,"right"); 
    


}

void xos_shell_cmd_init()
{
    show_os_logo();
    shell_cmd_head_init();
    shell_ctrl_key_init();
    spcial_key_cmd_init();
    xos_shell_cmd_register("test",test_shell_cmd,"test","help","test cmd");
    
    xos_shell_cmd_register("cat", cat_cmd, 
    "cat [dirname|filename]", 
    "cat [dirname|filename]", 
    "This command cat the file conent\n");

    xos_shell_cmd_register("pwd", pwd_cmd, 
    "pwd", 
    "pwd", 
    "This command check current path\n");
    
    xos_shell_cmd_register("cd", cd_cmd, 
    "cd dirname", 
    "cd dirname", 
    "This command change the dir\n");
    
    xos_shell_cmd_register("ls", ls_cmd, 
    "ls [dirname]", 
    "ls [dirname]", 
    "This command change the dir\n");

    xos_shell_cmd_register("exec", load_exec_cmd, 
    "exec [exec_path_name]", 
    "exec [exec_path_name]", 
    "This command fork child process and replace\n");

}

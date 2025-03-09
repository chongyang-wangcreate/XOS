
int get_argv_env_count(char *const *arg)
{
    int i = 0;
    while(arg[i] != NULL){
        i++;
    }
    return i;
}
void do_exec(const char *name)
{
    elf_load(name);
}

/*
    涉及到替换当前进程地址空间
    或者我们这样，给加载的新进程创建新的地址空间，但是某些数据继承自当前进程
    新进程创建完毕之后，销毁原进程地址空间
*/
static int do_sys_execve(const char *pathname, char *const argv[], char *const envp[])
{
    /*
       user space argv
       user space envp
    */
    int i;
    int argv_count = get_argv_env_count(argv);
    int envp_count = get_argv_env_count(envp);

    /*
        alloc kernel space temporary store
    */
    char **kern_argv = xos_kmalloc(sizeof(char *)*argv_count);
    char **kern_envp = xos_kmalloc(sizeof(char *)*envp_count);

    for(i = 0;i < argv_count;i++){
        kern_argv[i] = xos_kmalloc(strlen[argv[i]+1);
    }

    for(i = 0;i < envp_count;i++){
        kern_argv[i] = xos_kmalloc(strlen[envp[i]+1);
    }
    /*
        do_exec();
        get_elf_entry
        xos_create_process(int (*user_entry)(void *data),
            void *data,
            char *name,
            int prio);
    */
}


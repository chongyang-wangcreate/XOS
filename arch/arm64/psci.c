


#define PSCI_CPU_ON		     0xC4000003
#define BOOT_TEXT_ENTRY_ADDR 0x40010000

int __invoke_psci_fn_hvc(u64 id, u64 arg1, u64 arg2, u64 arg3);


void arch_psci_cpu_on(unsigned long fun_id,unsigned int cpu_id,unsigned long entry)
{
    __invoke_psci_fn_hvc(fun_id,cpu_id,entry);
}



void __invoke_psci_fn(unsigned int cpu_id)
{
    arch_psci_cpu_on(PSCI_CPU_ON,cpu_id,BOOT_TEXT_ENTRY_ADDR);  //当前从核启动只能指定固定的物理地址
}


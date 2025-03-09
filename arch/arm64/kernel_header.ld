OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

/*
	参数设置可动态调整
*/
KERNEL_START = 0xFFFFFF8000000000;
BOOT_MAIN_STACK_SIZE = 0x2000;
BOOT_MEM_SIZE   = 0x10000;
PAGE_SIZE = 4096;
PAGE_TABLE_RAM_SIZE = 0x2000000;
/*user_virtual
lma_user  user phy
*/
USER_VADDR_START = 0x08048000;
USER_VRANGE_SIZE = 8192K;

LOAD_ADDR_START = 0x40010000;
KERNEL_LINEAR_START = KERNEL_START + LOAD_ADDR_START;

MEMORY
{
	user_vma : ORIGIN = USER_VADDR_START, LENGTH = USER_VRANGE_SIZE
	phy_addr_boot : ORIGIN = LOAD_ADDR_START, LENGTH = 256K
	phy_addr_kernel : ORIGIN = 0x40030000, LENGTH = 40960K
	user_pma : ORIGIN = ORIGIN(phy_addr_kernel) + LENGTH(phy_addr_kernel), LENGTH = LENGTH(user_vma)
	vma_kernel_mapping : ORIGIN = KERNEL_START + ORIGIN(phy_addr_kernel), LENGTH = 40960K
}

PROVIDE (_user_pma = KERNEL_START + ORIGIN(user_pma));
PROVIDE (_user_vma = KERNEL_START + ORIGIN(user_vma));

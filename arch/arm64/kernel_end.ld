.kernel_text : { *(.text) } >vma_kernel_mapping AT>phy_addr_kernel
	.kernel_rodata . : { *(.rodata) } >vma_kernel_mapping AT>phy_addr_kernel
	.kernel_data . : { *(.data) } > vma_kernel_mapping AT>phy_addr_kernel
	.kernel_bss . : { PROVIDE (k_bss_start = .);
	  
	 *(.bss .bss.* COMMON)	  

    . = ALIGN(0x1000);
     PROVIDE (k_bss_end = .); } > vma_kernel_mapping AT>phy_addr_kernel

}
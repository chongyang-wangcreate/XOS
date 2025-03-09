SECTIONS{

		.user_text : { PROVIDE(user_text_start = .);
		output/usr/*(.text)
		PROVIDE(user_text_end = ALIGN(PAGE_SIZE));
		. = user_text_end; } >user_vma AT>user_pma
		PROVIDE (_user_text_saddr = KERNEL_START + user_text_start);
		PROVIDE (_user_text_eaddr = KERNEL_START + user_text_end);
	    .user_rodata : { PROVIDE(user_rodata_start = .);
		output/usr/*(.rodata)
		PROVIDE(user_rodata_end = ALIGN(PAGE_SIZE));
		. = user_rodata_end; } >user_vma AT>user_pma
		PROVIDE (_user_rodata_saddr = KERNEL_START + user_rodata_start);
		PROVIDE (_user_rodata_eaddr = KERNEL_START + user_rodata_end);
	.user_data : { PROVIDE(user_data_start = .);
		output/usr/*(.data)
		PROVIDE(user_data_end = ALIGN(PAGE_SIZE));
		. = user_data_end; } >user_vma AT>user_pma
		PROVIDE (_user_data_saddr = KERNEL_START + user_data_start);
		PROVIDE (_user_data_eaddr = KERNEL_START + user_data_end);
	.user_bss : { PROVIDE(user_bss_start = .);
		output/usr/*(.bss)
		PROVIDE(user_bss_end = ALIGN(PAGE_SIZE));
		. = user_bss_end; } >user_vma AT>user_pma
		PROVIDE (_user_bss_saddr = KERNEL_START + user_bss_start);
		PROVIDE (_user_bss_eaddr = KERNEL_START + user_bss_end);

		.boot_text : {   output/arch/arm64/entry.o(.text)
					output/arch/arm64/arch64_mmu.o(.text)
					output/arch/arm64/boot_mem.o(.text)
				    output/init/init.o(.text )
				    output/mm/setup_map.o(.text) 
				} >phy_addr_boot AT>phy_addr_boot
		.boot_rodata : { output/arch/arm64/entry.o(.rodata)
					output/arch/arm64/boot_mem.o(.rodata)
					output/arch/arm64/arch64_mmu.o(.rodata)
				    output/init/init.o(.rodata)
				    output/mm/setup_map.o(.rodata) 
				} >phy_addr_boot AT>phy_addr_boot		
		.boot_data : {   output/arch/arm64/entry.o(.data)
					output/arch/arm64/boot_mem.o(.data)
					output/arch/arm64/arch64_mmu.o(.data)					
				    output/init/init.o(.data)
				    output/mm/setup_map.o(.data) 
				} >phy_addr_boot AT>phy_addr_boot	
		.boot_bss : {  	
					PROVIDE(bss_start = .);
					output/arch/arm64/entry.o(.bss)
					output/arch/arm64/boot_mem.o(.bss)
					output/arch/arm64/arch64_mmu.o(.bss)				
				    output/init/init.o(.bss )
				    output/mm/setup_map.o(.bss) 

				    . = ALIGN(0x1000);
				    . += BOOT_MAIN_STACK_SIZE;

				    PROVIDE (_boot_main_stack_top = .);

					PROVIDE (_boot_mem_begin = .);
					. += BOOT_MEM_SIZE;

				    PROVIDE(bss_end = .);
				} >phy_addr_boot AT>phy_addr_boot	

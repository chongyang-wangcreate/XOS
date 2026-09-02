OUTPUT_FORMAT("elf64-littleaarch64")
OUTPUT_ARCH(aarch64)
ENTRY(_start)

/* ========== 参数设置 ========== */
KERNEL_START = 0xFFFFFF8000000000;
//BOOT_MAIN_STACK_SIZE = 0x4000;
BOOT_MEM_SIZE = 0x200000;
PAGE_SIZE = 4096;
PAGE_TABLE_RAM_SIZE = 0x2000000;

/* 用户空间参数 */
USER_VADDR_START = 0x08048000;
USER_VRANGE_SIZE = 8192K;

/* 加载地址 */
LOAD_ADDR_START = 0x40000000;
KERNEL_LINEAR_START = KERNEL_START + LOAD_ADDR_START;

/* 主栈大小（保留原有定义） */
MAIN_STACK_SIZE = 0x10000;

SECTIONS {
    /* ========== 内核虚拟地址起始 ========== */
    . = 0xffffff8040000000;
    __text = .;

    /* ---------- 启动代码（.text.boot）---------- */
    .text.boot : AT(ADDR(.text.boot) - 0xffffff8000000000) {
        __head_start = .;
        KEEP(*(.text.boot))
        __head_end = .;
    }

    . = ALIGN(4096);

    
    .text : AT(ADDR(.text) - 0xffffff8000000000) {
        *(EXCLUDE_FILE(output/usr/*.o output/usr/libc/*.o).text)
        *(EXCLUDE_FILE(output/usr/*.o output/usr/libc/*.o).text.*)
    }
    . = ALIGN(4096);
    ktext_end = .;
    _etext = .;

   
    .rodata : AT(ADDR(.rodata) - 0xffffff8000000000) {
        _srodata = .;
        *(EXCLUDE_FILE(output/usr/*.o output/usr/libc/*.o).rodata)
        *(EXCLUDE_FILE(output/usr/*.o output/usr/libc/*.o).rodata.*)
        . = ALIGN(4096);
        _erodata = .;
    }
    . = ALIGN(4096);
    .data : AT(ADDR(.data) - 0xffffff8000000000) {
        _sdata = .;
        *(EXCLUDE_FILE(output/usr/*.o output/usr/libc/*.o).data)
        *(EXCLUDE_FILE(output/usr/*.o output/usr/libc/*.o).data.*)
        _edata = .;
    }

    . = ALIGN(4096);
    .bss : AT(ADDR(.bss) - 0xffffff8000000000) {
        __bss_start = .;
        *(EXCLUDE_FILE(output/usr/*.o output/usr/libc/*.o).bss)
        *(EXCLUDE_FILE(output/usr/*.o output/usr/libc/*.o).bss.*)
        *(EXCLUDE_FILE(output/usr/*.o output/usr/libc/*.o) COMMON)
        __bss_end = .;

    }
    __bss_stop = .;
     
    . = ALIGN(4096);
    __init_begin = .;
    .kernel_reserved : AT(ADDR(.kernel_reserved) - 0xffffff8000000000) {
        /* 主栈 */
        . = ALIGN(16);
        . += MAIN_STACK_SIZE;
        PROVIDE(main_stack_top = .);

        /* 页表 */
        . = ALIGN(4096);
        PROVIDE(l1_kernel_pgt = .);
        . += 4096;
        PROVIDE(l2_kernel_pgt0 = .);
        . += 4096;
        PROVIDE(l2_kernel_pgt1 = .);
        . += 4096;
        PROVIDE(l1_user_pgt = .);
        . += 4096;
        PROVIDE(l2_user_pgt = .);
        . += 4096;

        /* Boot 内存分配器 */
        . = ALIGN(4096);
        PROVIDE(_boot_mem_begin = .);
        . += BOOT_MEM_SIZE;

        /* Zone 元数据 */
        . = ALIGN(4096);
        PROVIDE(_zone_metadata_start = .);
        . += 0x200000;          /* 2MB */
        PROVIDE(_zone_metadata_end = .);

        . = ALIGN(4096);
        PROVIDE(_page_arrays_end = .);
    }
    __init_end = .;
    PROVIDE(kernel_end = ADDR(.kernel_reserved) + SIZEOF(.kernel_reserved));
    
    kernel_end_phys = kernel_end - 0xffffff8000000000;
    __bss_start_phys = __bss_start - 0xffffff8000000000;
    __bss_end_phys = __bss_end - 0xffffff8000000000;


    /* 用户段虚拟地址 */
    . = USER_VADDR_START;
    user_lma_base = ALIGN(LOADADDR(.kernel_reserved) + SIZEOF(.kernel_reserved),PAGE_SIZE);


    .user_text : AT(user_lma_base) {
        PROVIDE(user_text_start = .);
        output/usr/*.o(.text .text.*)
        toutput/usr/libc/*.o(.text .text.*)
        PROVIDE(user_text_end = ALIGN(PAGE_SIZE));
        . = user_text_end;
    }
    PROVIDE(_user_text_saddr = user_text_start);   
    PROVIDE(_user_text_eaddr = user_text_end);
    PROVIDE(_user_text_lma = LOADADDR(.user_text));

    .user_rodata : AT(ALIGN(LOADADDR(.user_text) +SIZEOF(.user_text), PAGE_SIZE)) {
        PROVIDE(user_rodata_start = .);
        output/usr/*.o(.rodata .rodata.*)
        output/usr/libc/*.o(.rodata .rodata.*)

        PROVIDE(user_rodata_end = ALIGN(PAGE_SIZE));
        . = user_rodata_end;
    }
    PROVIDE(_user_rodata_saddr = user_rodata_start);
    PROVIDE(_user_rodata_eaddr = user_rodata_end);
    PROVIDE(_user_rodata_lma = LOADADDR(.user_rodata));

    .user_data : AT(ALIGN(LOADADDR(.user_rodata) +SIZEOF(.user_rodata), PAGE_SIZE)) {
        PROVIDE(user_data_start = .);
        output/usr/*.o(.data .data.*)
        output/usr/libc/*.o(.data .data.*)
        PROVIDE(user_data_end = ALIGN(PAGE_SIZE));
        . = user_data_end;
    }
    PROVIDE(_user_data_saddr = user_data_start);
    PROVIDE(_user_data_eaddr = user_data_end);
    PROVIDE(_user_data_lma = LOADADDR(.user_data));

    .user_bss : AT(ALIGN(LOADADDR(.user_data) +SIZEOF(.user_data), PAGE_SIZE)) {
        PROVIDE(user_bss_start = .);
        output/usr/*.o(.bss .bss.* COMMON)
        output/usr/libc/*.o(.bss .bss.* COMMON)
        PROVIDE(user_bss_end = ALIGN(PAGE_SIZE));
        . = user_bss_end;
    }
    PROVIDE(_user_bss_saddr = user_bss_start);
    PROVIDE(_user_bss_eaddr = user_bss_end);
    PROVIDE(_user_bss_lma = LOADADDR(.user_bss));
    PROVIDE(_user_image_end_lma = ALIGN(LOADADDR(.user_bss) + SIZEOF(.user_bss), PAGE_SIZE));
    PROVIDE(_user_end_lma_virt = KERNEL_START + _user_image_end_lma);
    PROVIDE (_user_vma = _user_text_saddr);
    PROVIDE (_user_lma = _user_text_lma);

    PROVIDE(_kernel_linear_start = KERNEL_LINEAR_START);
    PROVIDE(_load_addr_start = LOAD_ADDR_START);
	PROVIDE(_load_addr_start_virt = KERNEL_START + _load_addr_start);
    PROVIDE(kernel_end_phys_virt = KERNEL_START + kernel_end_phys); 
	PROVIDE(_boot_mem_begin_phys = _boot_mem_begin - 0xffffff8000000000);
        
    PROVIDE(_page_arrays_end_phys = _page_arrays_end - 0xffffff8000000000);
    /DISCARD/ : {
        *(.comment)
        *(.eh_frame)
    }
}

__bss_size = (__bss_end - __bss_start) >> 3;

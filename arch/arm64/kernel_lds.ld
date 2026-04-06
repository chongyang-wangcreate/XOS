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

KERNEL_PAGE_ARRAY_SIZE = (64*1024*1024);
USER_PAGE_ARRAY_SIZE = (128*1024*1024);
DMA_PAGE_ARRAY_SIZE = (32*1024*1024);

SECTIONS {
    /* ========== 内核虚拟地址起始 ========== */
    . = 0xffffff8040000000;

    /* ---------- 启动代码（.text.boot）---------- */
    .text.boot : AT(ADDR(.text.boot) - 0xffffff8000000000) {
        KEEP(*(.text.boot))
    }

    . = ALIGN(4096);

    /* ---------- 主代码段（.text）---------- */
    .text : AT(ADDR(.text) - 0xffffff8000000000) {
        *(.text) *(.text.*)
    }
    ktext_end = .;

    /* ---------- 只读数据 ---------- */
    .rodata : AT(ADDR(.rodata) - 0xffffff8000000000) {
        *(.rodata) *(.rodata.*)
    }

    /* ---------- 数据段 ---------- */
    .data : AT(ADDR(.data) - 0xffffff8000000000) {
        *(.data) *(.data.*)
    }

    /* ---------- BSS 段 + 栈 ---------- */
    . = ALIGN(4096);
    .bss : AT(ADDR(.bss) - 0xffffff8000000000) {
        __bss_start = .;
        *(.bss .bss.*)
        __bss_end = .;

    }

     /* ========== 内核预留区域（栈、页表、内存池等）========== */
    . = ALIGN(4096);
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

        /* 内核页描述符数组 */
        . = ALIGN(4096);
        PROVIDE(_kernel_page_array_start = .);
        PROVIDE(_kernel_page_array = .);
        . += KERNEL_PAGE_ARRAY_SIZE;
        PROVIDE(_kernel_page_array_end = .);

        /* 用户页描述符数组 */
        . = ALIGN(4096);
        PROVIDE(_user_page_array_start = .);
        PROVIDE(_user_page_array = .);
        . += USER_PAGE_ARRAY_SIZE;
        PROVIDE(_user_page_array_end = .);

        /* DMA 页描述符数组 */
        . = ALIGN(4096);
        PROVIDE(_dma_page_array_start = .);
        PROVIDE(_dma_page_array = .);
        . += DMA_PAGE_ARRAY_SIZE;
        PROVIDE(_dma_page_array_end = .);

        /* 所有页描述符结束 */
        . = ALIGN(4096);
        PROVIDE(_page_arrays_end = .);
    }

    PROVIDE(kernel_end = ADDR(.kernel_reserved) + SIZEOF(.kernel_reserved));
    /* ========== 物理地址符号（原有）========== */
    kernel_end_phys = kernel_end - 0xffffff8000000000;
    __bss_start_phys = __bss_start - 0xffffff8000000000;
    __bss_end_phys = __bss_end - 0xffffff8000000000;

    /* ========== 新增：用户段（简化版）========== */
    /* 用户段虚拟地址（高地址空间） */
    . = KERNEL_START + USER_VADDR_START;

    .user_text : AT(ADDR(.user_text) - KERNEL_START) {
        PROVIDE(user_text_start = .);
        *(.user.text)
        PROVIDE(user_text_end = ALIGN(PAGE_SIZE));
        . = user_text_end;
    }
    PROVIDE(_user_text_saddr = user_text_start);   
    PROVIDE(_user_text_eaddr = user_text_end);

    .user_rodata : AT(ADDR(.user_rodata) - KERNEL_START) {
        PROVIDE(user_rodata_start = .);
        *(.user.rodata)
        PROVIDE(user_rodata_end = ALIGN(PAGE_SIZE));
        . = user_rodata_end;
    }
    PROVIDE(_user_rodata_saddr = user_rodata_start);
    PROVIDE(_user_rodata_eaddr = user_rodata_end);

    .user_data : AT(ADDR(.user_data) - KERNEL_START) {
        PROVIDE(user_data_start = .);
        *(.user.data)
        PROVIDE(user_data_end = ALIGN(PAGE_SIZE));
        . = user_data_end;
    }
    PROVIDE(_user_data_saddr = user_data_start);
    PROVIDE(_user_data_eaddr = user_data_end);

    .user_bss : AT(ADDR(.user_bss) - KERNEL_START) {
        PROVIDE(user_bss_start = .);
        *(.user.bss)
        PROVIDE(user_bss_end = ALIGN(PAGE_SIZE));
        . = user_bss_end;
    }
    PROVIDE(_user_bss_saddr = user_bss_start);
    PROVIDE(_user_bss_eaddr = user_bss_end);
    PROVIDE (_user_vma = _user_text_saddr);
    PROVIDE (_user_lma = _user_vma - KERNEL_START);

    /* ========== 新增：内核线性映射相关符号 ========== */
    PROVIDE(_kernel_linear_start = KERNEL_LINEAR_START);
    PROVIDE(_load_addr_start = LOAD_ADDR_START);
	PROVIDE(_load_addr_start_virt = KERNEL_START + _load_addr_start);
    PROVIDE(kernel_end_phys_virt = KERNEL_START + kernel_end_phys); 
	PROVIDE(_boot_mem_begin_phys = _boot_mem_begin - 0xffffff8000000000);
        /* 在 /DISCARD/ 之前添加 */
    PROVIDE(_page_arrays_end_phys = _page_arrays_end - 0xffffff8000000000);
    /DISCARD/ : {
        *(.comment)
    }
}

/* ========== 辅助计算符号 ========== */
__bss_size = (__bss_end - __bss_start) >> 3;
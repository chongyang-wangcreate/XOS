QEMU = qemu-system-arm
CROSSCOMPILE := aarch64-linux-gnu-
#CROSSCOMPILE := aarch64-none-elf-
# CROSSCOMPILE := arm-linux-gnueabi-
CC = $(CROSSCOMPILE)gcc
AS = $(CROSSCOMPILE)as
LD = $(CROSSCOMPILE)ld
OBJCOPY = $(CROSSCOMPILE)objcopy
OBJDUMP = $(CROSSCOMPILE)objdump

#CFLAGS += -march=armv8-a -mtune=cortex-a57 -fno-pic -static -fno-builtin -fno-strict-aliasing -Wall -Werror -I. -g 
CFLAGS := \
    -march=armv8-a \
    -mtune=cortex-a57 \
    -ffreestanding \
    -fno-pic \
    -fno-builtin \
    -fno-strict-aliasing \
    -Wall \
    -Werror \
    -I. -g \
    -I./mm \
    -I./include \
    -I./include/fsh \
    -I./kernel/include \
    -I./arch/arm64/include \
    -I./drivers/include \
    -I./lib \
    -I./fs \
    -I./usr/libc/include/ \
    -I./usr \
   # -nostdinc
LDFLAGS = -L. -L/mm -L./kernel/init -L./kernel -L./arch/arm64
ASFLAGS = -march=armv8-a

LIBGCC = $(shell $(CC) -print-libgcc-file-name)

# host compiler
HOSTCC_preferred = gcc
define get_hostcc
    $(if $(shell which $(HOSTCC_preferred)),$(HOSTCC_preferred),"cc")
endef
HOSTCC := $(call get_hostcc)

# general rules
quiet-command = $(if $(V),$1,$(if $(2),@echo $2 && $1, @$1))

LINK_BIN = $(call quiet-command,$(LD) $(LDFLAGS) \
    -T $(1) -o $(2) $(3) $(LIBS) -b binary $(4), "  LINK     $(TARGET_DIR)$@")

LINK_INIT = $(call quiet-command,$(LD) $(LDFLAGS) \
    $(1) -o $@.out $<, "  LINK     $(TARGET_DIR)$@")
OBJCOPY_INIT = $(call quiet-command,$(OBJCOPY) \
    -S -O binary --prefix-symbols="_binary_$@" $@.out $@, "  OBJCOPY  $(TARGET_DIR)$@")
BUILD_DIRS = \
    output \
    output/device \
    output/lib \
    output/mm \
    output/init \
    output/kernel/syscall \
    output/arch/arm64 \
    output/usr \
    output/usr/libc \
    output/fs \
    output/fs/sysfs \
    output/fs/tmpfs \
    output/drivers \
    output/drivers/console \
    output/drivers/base


create_build_dirs = $(foreach dir,$(BUILD_DIRS),$(shell mkdir -p $(dir)))

build-directory += $(call create_build_dirs)

output/%.o: %.c
	$(call build-directory )
	$(call quiet-command,$(CC) $(CFLAGS) \
		-c -o $@ $<,"  CC       $(TARGET_DIR)$@")

AS_WITH = $(call quiet-command,$(CC) $(ASFLAGS) \
        $(1) -c -o $@ $<,"  AS       $(TARGET_DIR)$@")

output/%.o: %.S
	$(call build-directory)
	$(call AS_WITH, )


LIBS = $(LIBGCC)

ARM64_OBJS = \
    arch/arm64/arch64_timer.o\
    arch/arm64/arch64_irq.o\
    arch/arm64/entry.o\
    arch/arm64/arch64_mmu.o\
    arch/arm64/exce.o\
    arch/arm64/boot_mem.o\
	arch/arm64/gic-v3.o

LIB_OBJS = \
    lib/string.o \
    lib/uaccess.o

TOBJS = \
    init/init.o\

DEV_OBJ = \
    device/uart.o \
    device/xos_dev.o

DRIV_OBJ = \
    drivers/xos_bus.o \
    drivers/xos_driver.o \
    drivers/base/platform.o\
    drivers/console/console.o 

MM_OBJ = \
    mm/setup_mm.o\
    mm/setup_map.o\
    mm/phy_mem.o\
    mm/xos_page.o\
    mm/xos_zone.o\
    mm/xos_cache.o

KERNEL_OBJ = \
    kernel/spinlock.o\
    kernel/main.o\
    kernel/bit_map.o\
    kernel/dup_printk.o\
    kernel/task.o\
    kernel/schedule.o\
    kernel/syscall/syscall.o\
    kernel/syscall/nano_sleep.o\
    kernel/syscall/fork.o\
    kernel/interrupt.o\
    kernel/cpu_desc.o\
    kernel/tick_timer.o\
    kernel/xos_sleep.o\
    kernel/vector.o\
    kernel/xos_debug.o\
    kernel/switch_user.o\
    kernel/xos_vmm.o\
    kernel/load_user2.o\
    kernel/xos_mutex.o\
    kernel/workqueue.o\
    kernel/xos_sem.o

FS_OBJ = \
    fs/mount.o\
    fs/xdentry.o\
    fs/xnode.o\
    fs/xos_fs_types.o\
    fs/xos_ramfs.o\
    fs/tmpfs/xos_tmpfs.o\
    fs/xos_vfs.o\
    fs/xos_char_dev.o\
    fs/xos_file.o \
    fs/xos_devfs.o \
    fs/readdir.o\
    fs/stat.o\
    fs/sysfs/sys_mkdir.o\
    fs/sysfs/sys_mknod.o\
    fs/sysfs/sys_open.o\
    fs/sysfs/sys_read.o\
    fs/sysfs/sys_write.o\
    fs/sysfs/sys_getcwd.o\
    fs/sysfs/sys_chdir.o

FS_TMP_OBJ = \
    fs/xos_file.o

USER_OBJ = \
    usr/stdlib.o \
    usr/usys.o \
    usr/shell_init.o\
    usr/shell_cmd.o\
    usr/shell.o\
    usr/libc/sleep.o\
    usr/libc/open.o\
    usr/libc/read.o\
    usr/libc/write.o\
    usr/libc/dup.o\
    usr/libc/mkdir.o\
    usr/libc/fork.o\
    usr/libc/readdir.o\
    usr/ls.o\
    usr/usr_printf.o
        #usr/printf.o

OBJS += $(MM_OBJ)
OBJS += $(LIB_OBJS)
OBJS += $(TOBJS)
OBJS += $(DEV_OBJ)
OBJS += $(KERNEL_OBJ)
OBJS += $(ARM64_OBJS)
OBJS += $(FS_OBJ)
#OBJS += $(FS_TMP_OBJ)
OBJS += $(USER_OBJ)
OBJS += $(DRIV_OBJ)

KERN_OBJS = $(OBJS) 
kernel.elf: $(addprefix output/,$(KERN_OBJS)) ./arch/arm64/kernel_lds.ld 
	$(call LINK_BIN, ./arch/arm64/kernel_lds.ld, kernel.elf, \
		$(addprefix output/,$(KERN_OBJS)), \
	    )
	$(OBJDUMP) -S kernel.elf > kernel.asm
	$(OBJDUMP) -t kernel.elf | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > kernel.sym

clean: 
	rm -rf output
	rm -f *.o *.d *.asm *.sym 
	rm -f kernel.asm
	rm -f kernel.elf


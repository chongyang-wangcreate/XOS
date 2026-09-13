#!/bin/bash

echo "Received argument: $1"

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
SMP_CPUS="${SMP_CPUS:-4}"
DTB_FILE="/tmp/xos_qemu_${SMP_CPUS}cpu.dtb"

build_dtb() {
    qemu-system-aarch64 \
        -machine "virt,gic-version=3,dumpdtb=$DTB_FILE" \
        -smp "$SMP_CPUS" -cpu cortex-a57 -m 1024 -nographic
}

case "$SMP_CPUS" in
    1|2|4) ;;
    *)
        echo "SMP_CPUS must be 1, 2, or 4."
        exit 1
        ;;
esac

build_dtb

if [ "$1" == "run" ]; then
    echo "Running QEMU in normal mode"
    clear
    cd "$ROOT_DIR"
    qemu-system-aarch64 -machine virt,gic-version=3 \
    -smp "$SMP_CPUS" -cpu cortex-a57 -m 1024 -nographic \
    -dtb "$DTB_FILE" \
    -device loader,file="$DTB_FILE",addr=0x58000000,force-raw=on \
    -kernel kernel.elf

elif [ "$1" == "run_debug" ]; then
    echo "Running QEMU in debug mode"
    clear
    cd "$ROOT_DIR"
    qemu-system-aarch64 -machine virt,gic-version=3 \
    -smp "$SMP_CPUS" -cpu cortex-a57 -m 1024 -nographic \
    -d exec,cpu,guest_errors,in_asm -D qemu.log \
    -dtb "$DTB_FILE" \
    -device loader,file="$DTB_FILE",addr=0x58000000,force-raw=on \
    -kernel kernel.elf

elif [ "$1" == "symbol" ]; then
    echo "Setting up GDB symbols"
    symbol kernel.elf
    set arch aarch64
    target remote tcp::1234

elif [ "$1" == "debug" ]; then
    echo "Running QEMU for GDB"
    clear
    cd "$ROOT_DIR"
    qemu-system-aarch64 -machine virt,gic-version=3 \
    -smp "$SMP_CPUS" -cpu cortex-a57 -m 1024 -nographic \
    -dtb "$DTB_FILE" \
    -device loader,file="$DTB_FILE",addr=0x58000000,force-raw=on \
    -kernel kernel.elf -s -S

else
    echo "Invalid argument. Use 'run', 'run_debug', or 'symbol'."
fi

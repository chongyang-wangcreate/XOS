#!/bin/bash

# 检查传入的参数
echo "Received argument: $1"

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
DTS_FILE="$ROOT_DIR/qemu.dts"
DTB_FILE="/tmp/xos_qemu_minimal.dtb"

if [ -f "$DTS_FILE" ] && { [ ! -f "$DTB_FILE" ] || [ "$DTS_FILE" -nt "$DTB_FILE" ]; }; then
    dtc -I dts -O dtb -o "$DTB_FILE" "$DTS_FILE"
fi

if [ $1 == "run" ]; then
    echo "Running QEMU in normal mode"
    export PATH=$PATH
    clear
    cd "$ROOT_DIR"
    qemu-system-aarch64 -machine virt,gic-version=3 -smp 1 -cpu cortex-a57 \
    -machine type=virt -m 1024 -nographic \
    -dtb "$DTB_FILE" \
    -device loader,file="$DTB_FILE",addr=0x58000000,force-raw=on \
    -kernel kernel.elf

elif [ "$1" == "run_debug" ]; then
    echo "Running QEMU in debug mode"
    export PATH=$PATH
    clear
    cd "$ROOT_DIR"
    qemu-system-aarch64 -machine virt -cpu cortex-a57 \
    -machine type=virt -m 1024 -nographic \
    -d exec,cpu,guest_errors,in_asm -D qemu.log -kernel kernel.elf

elif [ "$1" == "symbol" ]; then
    echo "Setting up GDB symbols"
    symbol kernel.elf
    set arch aarch64
    target remote tcp::1234

elif [ "$1" == "debug" ]; then
    echo "Setting up GDB symbols"
    export PATH=$PATH
    clear
    cd "$ROOT_DIR"
    qemu-system-aarch64 -machine virt,gic-version=3 -smp 1 -cpu cortex-a57 \
    -machine type=virt -m 1024 -nographic \
    -dtb "$DTB_FILE" \
    -device loader,file="$DTB_FILE",addr=0x58000000,force-raw=on \
    -kernel kernel.elf -s -S

else
    echo "Invalid argument. Use 'run', 'run_debug', or 'symbol'."
fi
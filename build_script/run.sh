#!/bin/bash

# 检查传入的参数
echo "Received argument: $1"
if [ $1 == "run" ]; then
    echo "Running QEMU in normal mode"
    export PATH=$PATH
    clear
    cd ..
    qemu-system-aarch64 -machine virt,gic-version=3 -smp 1 -cpu cortex-a57 \
    -machine type=virt -m 1024 -nographic \
    -singlestep -kernel kernel.elf

elif [ "$1" == "run_debug" ]; then
    echo "Running QEMU in debug mode"
    export PATH=$PATH
    clear
    cd ..
    qemu-system-aarch64 -machine virt -cpu cortex-a57 \
    -machine type=virt -m 1024 -nographic \
    -singlestep \
    -d exec,cpu,guest_errors,in_asm -D qemu.log -kernel kernel.elf

elif [ "$1" == "symbol" ]; then
    echo "Setting up GDB symbols"
    symbol kernel.elf
    set arch aarch64
    target remote tcp::1234

else
    echo "Invalid argument. Use 'run', 'run_debug', or 'symbol'."
fi

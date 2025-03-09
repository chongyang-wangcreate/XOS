#!/bin/bash

# 输出传递给脚本的所有参数
echo "You entered: $@"  # 使用 "$@" 输出所有传递的参数

# 获取当前目录
current_dir=$(pwd)

# 定义上层目录路径
parent_dir=$(dirname "$current_dir")

# 检查输入参数
if [ $# -eq 0 ]; then
    echo "Error: No argument provided. Please use 'make' or 'clean'."
    exit 1
fi

# 根据参数执行相应的操作
echo "Argument: \$1"
case $1 in
   "make")
        # 先执行 kernel_lds.sh
        echo "Executing kernel_lds.sh..."
        cd ../arch/arm64 || { echo "Failed to change directory to ../arch/arm64"; exit 1; }
        ./kernel_lds.sh || { echo "Failed to execute kernel_lds.sh"; exit 1; }

        # 返回到原目录
        cd "$current_dir" || { echo "Failed to return to the original directory"; exit 1; }

        # 复制 Makefile 和 Makefile.inc 到上层目录
        cp "$current_dir/Makefile" "$parent_dir/"
   

        # 进入上层目录
        cd "$parent_dir" || exit

        # 执行 make 命令
        make

        # 执行完后，执行 rm_lds.sh
        cd ./arch/arm64 || { echo "Failed to change directory to ../arch/arm64"; exit 1; }
        ./rm_lds.sh || { echo "Failed to execute rm_lds.sh"; exit 1; }

        # 打印完成信息
        echo "Makefile has been copied to $parent_dir and 'make' has been executed."
        rm -f "$parent_dir/Makefile" 
        ;;

    "clean")
        # 先执行 rm_lds.sh
        echo "Executing rm_lds.sh..."
        cd ../arch/arm64 || { echo "Failed to change directory to ../arch/arm64"; exit 1; }
       # ./rm_lds.sh || { echo "Failed to execute rm_lds.sh"; exit 1; }

        # 返回到原目录
        cd "$current_dir" || { echo "Failed to return to the original directory"; exit 1; }

        # 复制 Makefile 和 Makefile.inc 到上层目录
        cp "$current_dir/Makefile" "$parent_dir/"


        # 进入上层目录
        cd "$parent_dir" || exit

        # 执行 make clean
        make clean

  
        rm -f "$parent_dir/Makefile" 

        # 打印完成信息
        echo "'make clean' has been executed and Makefile has been removed from $parent_dir."
        ;;

    *)
        echo "Invalid argument. Please use 'make' or 'clean'."
        exit 1
        ;;
esac

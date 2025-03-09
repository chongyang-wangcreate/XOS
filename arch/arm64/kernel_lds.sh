#!/bin/bash

# 定义文件路径
header_file="kernel_header.ld"
sections_file="kernel_section.ld"
kernel_file="kernel_end.ld"
output_file="kernel_lds.ld"

# 创建并写入 header 部分
#echo "Creating $output_file with content from $header_file"
cat $header_file > $output_file

# 添加 sections 部分
#echo "Appending sections to $output_file"
cat $sections_file >> $output_file

# 添加 kernel 部分
#echo "Appending kernel section to $output_file"
cat $kernel_file >> $output_file


# kernel_inject
kernel inject driver module, support register read/write, suspend and resume register inject function

# Compile
Build kernel_inject.c as a ko module.

Add kernel_inject.c into kernel driver Makefile.
```
obj-m := kernel_inject.o
```

# Usage
1. `adb push kernel_inject.ko /data/`
2. `insmod /data/kernel_inject.ko`
3. `cd /sys/kernel/debug/inject`
4. `ls -l`
-rw------- 1 root root 0 2023-01-06 09:56 reg
-rw------- 1 root root 0 2023-01-06 09:56 resume_reg_cfg
-rw------- 1 root root 0 2023-01-06 09:56 suspend_reg_cfg

5. 查看寄存器读写使用方法：`cat reg`
```
Usage: echo <CMD> <REG> <VAL> > <DEBUGFS>/inject/reg

Example 1: read one register
  echo "r 0x3451008c 1" > /d/inject/reg

Example 2: read multy registers
  echo "r 0x3451008c 10" > /d/inject/reg

Example 3: write register
  echo "w 0x3451008c 0x2" > /d/inject/reg
```
6. 配置进入suspend操作的寄存器
```
Example 1: suspend时将寄存器0x3451008c修改为0x2
    echo "0x3451008c 0x2 > /d/inject/suspend_reg_cfg
```
使用命令`cat /d/inject/suspend_reg_cfg`可以查看配置的寄存器序列

7. 配置resume操作的寄存器
```
Example 1: resume时将寄存器0x3451008c修改为0x1
    echo "0x3451008c 0x1 > /d/inject/resume_reg_cfg
```
使用命令`cat /d/inject/resume_reg_cfg`可以查看配置的寄存器序列

Modified file:
1. 在 edk2/HardwarePkg 目录下编写 ``Hardwareinfo.c``, ``Hardwareinfo.inf``, ``HardwarePkg.dsc``。注册新的自定义 ACPI 表，将硬件运行时信息放入该 ACPI 表
2. 编译 HardwarePkg
3. 修改内核，在 drivers/acpi/ 目录下添加 ``get_hwinfo.c``。在系统启动时读取保存了硬件信息的 ACPI 表，往 sysfs 中添加硬件运行信息的文件节点
4. 在 drivers/acpi/ 目录下的 Makefile 中添加 ``obj-y += get_hwinfo.o``

Test:
修改 startup.nsh，调用 My_Hardwareinfo.efi
然后在作业根目录下调用 ./run_uefi.sh
进入系统后 cat 一下对应的文件
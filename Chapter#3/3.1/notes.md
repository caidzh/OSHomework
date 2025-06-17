Modified file:
1. 在 edk2/MyPkg 文件夹下创建 ``ReadACPI.c``, ``ReadACPI.inf``, ``MyPkg.dsc``
2. 编译 MyPkg
3. 找到 ReadACPI.efi 并挂载到文件系统内，启动 qemu 进行测试

Test:
修改 startup.nsh，调用 My_AcpiView.efi
然后在作业根目录下调用 ./run_uefi.sh
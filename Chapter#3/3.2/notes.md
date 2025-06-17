Modified file:
1. 在 edk2/MyPkg 文件夹下创建 ``ChangeACPI.c``, ``ChangeACPI.inf``, 修改 ``MyPkg.dsc``
2. 编译 MyPkg
3. 将 ChangeACPI.efi 并挂载到文件系统内，启动 qemu 进行测试

Test:
修改 startup.nsh，调用 My_ChangeAcpi.efi
然后在作业根目录下调用 ./run_uefi.sh
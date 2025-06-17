#include <Uefi.h>
#include <Library/UefiLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <IndustryStandard/Acpi.h>
#include <Protocol/AcpiSystemDescriptionTable.h>

/**
  计算ACPI表的校验和
  
  @param[in] Buffer   表的起始地址
  @param[in] Length   表的长度
  
  @return 校验和
**/
UINT8
CalculateChecksum (
  IN UINT8                           *Buffer,
  IN UINTN                           Length
  )
{
  UINTN                 Index;
  UINT8                 Sum = 0;
  
  for (Index = 0; Index < Length; Index++) {
    Sum = (UINT8)(Sum + Buffer[Index]);
  }
  
  return Sum;
}

/**
  打印所有ACPI表信息
**/
VOID
EFIAPI
PrintAllACPITables (
  VOID
  )
{
  EFI_STATUS                        Status;
  EFI_ACPI_SDT_PROTOCOL            *AcpiSdt;
  EFI_ACPI_SDT_HEADER             *Table;
  UINTN                            TableSize;
  EFI_ACPI_TABLE_VERSION          TableVersion;
  UINT8                            Checksum;
  
  //获取ACPI SDT协议
  Status = gBS->LocateProtocol (
                  &gEfiAcpiSdtProtocolGuid,
                  NULL,
                  (VOID **)&AcpiSdt
                  );
  if (EFI_ERROR (Status)) {
    Print (L"Failed to locate ACPI SDT protocol: %r\n", Status);
    return;
  }

  UINTN Index = 0;
  
  while(TRUE) {
    Status = AcpiSdt->GetAcpiTable (
                        Index,
                        &Table,
                        &TableVersion,
                        &TableSize
                        );
                        
    if (EFI_ERROR (Status)) {
      break;
    }
    
    Checksum = CalculateChecksum ((UINT8*)Table, Table->Length);
    
    // 打印表信息
    Print (L"Table Address: 0x%016lx\n", (UINT64)Table);
    Print (L"Table Length: %d\n", Table->Length);
    Print (L"OEM ID: %c%c%c%c%c%c\n",
           Table->OemId[0], Table->OemId[1], Table->OemId[2],
           Table->OemId[3], Table->OemId[4], Table->OemId[5]);
    Print (L"Checksum: 0x%02x\n", Checksum);
    Print (L"\n");

    Index++;
  }
}

/**
  UEFI应用程序入口点

  @param[in] ImageHandle    加载镜像的句柄
  @param[in] SystemTable    指向系统表的指针

  @retval EFI_SUCCESS      程序执行成功
  @retval Other            发生错误
**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  Print (L"\nACPI Tables Information:\n");
  Print (L"=======================\n\n");
  
  PrintAllACPITables();
  
  return EFI_SUCCESS;
}
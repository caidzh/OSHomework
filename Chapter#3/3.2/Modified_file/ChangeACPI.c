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
    
    // 打印表头信息
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
  打印ACPI表头信息
  @param[in] Table    指向ACPI表头的指针
**/
VOID
PrintAcpiTableHeader (
  EFI_ACPI_SDT_HEADER *Table
  )
{
  EFI_ACPI_DESCRIPTION_HEADER *Header = (EFI_ACPI_DESCRIPTION_HEADER *)Table;
  
  Print(L"Signature      : %c%c%c%c\n",
        (Header->Signature & 0xFF),
        (Header->Signature >> 8) & 0xFF,
        (Header->Signature >> 16) & 0xFF,
        (Header->Signature >> 24) & 0xFF);
  Print(L"Length         : %d\n", Header->Length);
  Print(L"Revision       : %d\n", Header->Revision);
  Print(L"Checksum       : 0x%02x\n", Header->Checksum);
  Print(L"OEM ID         : %c%c%c%c%c%c\n",
        Header->OemId[0], Header->OemId[1], Header->OemId[2],
        Header->OemId[3], Header->OemId[4], Header->OemId[5]);
  UINT8 *OemTableId = (UINT8 *)&Header->OemTableId;
  Print(L"OEM Table ID   : %c%c%c%c%c%c%c%c\n",
      OemTableId[0], OemTableId[1], OemTableId[2], OemTableId[3],
      OemTableId[4], OemTableId[5], OemTableId[6], OemTableId[7]);
  Print(L"OEM Revision   : 0x%08x\n", Header->OemRevision);
  Print(L"Creator ID     : %c%c%c%c\n",
        (Header->CreatorId & 0xFF),
        (Header->CreatorId >> 8) & 0xFF,
        (Header->CreatorId >> 16) & 0xFF,
        (Header->CreatorId >> 24) & 0xFF);
  Print(L"Creator Rev.   : 0x%08x\n", Header->CreatorRevision);
}

EFI_STATUS
EFIAPI
ChangeACPITable (
  IN UINTN TableIndex,
  IN EFI_ACPI_DESCRIPTION_HEADER *NewHeader
  )
{
  EFI_STATUS Status;
  EFI_ACPI_SDT_PROTOCOL *AcpiSdt;
  EFI_ACPI_SDT_HEADER *Table;
  UINTN TableSize;
  EFI_ACPI_TABLE_VERSION TableVersion;

  Status = gBS->LocateProtocol(
    &gEfiAcpiSdtProtocolGuid,
    NULL,
    (VOID **)&AcpiSdt
  );
  if (EFI_ERROR(Status)) {
    Print(L"Failed to locate ACPI SDT protocol: %r\n", Status);
    return Status;
  }

  Status = AcpiSdt->GetAcpiTable(
    TableIndex,
    &Table,
    &TableVersion,
    &TableSize
  );
  if (EFI_ERROR(Status)) {
    Print(L"Failed to get ACPI table %d: %r\n", TableIndex, Status);
    return Status;
  }

  Print(L"Initial ACPI Table Header information:\n");
  PrintAcpiTableHeader(Table);

  EFI_ACPI_DESCRIPTION_HEADER *Table_Header;
  Table_Header = (EFI_ACPI_DESCRIPTION_HEADER *)Table;
  Table_Header->OemId[0] = 'O';
  Table_Header->OemId[1] = 'A';
  Table_Header->OemId[2] = 'K';
  Table_Header->OemId[3] = 'I';
  Table_Header->OemId[4] = 'O';
  Table_Header->OemId[5] = 'I';

  // CopyMem(Table_Header, NewHeader, NewHeader->Length);

  Table->Checksum = 0;
  Table->Checksum = (UINT8)(0 - CalculateChecksum((UINT8*)Table, Table->Length));

  Print(L"ACPI Table %d modified and checksum updated.\n", TableIndex);
  Print(L"Modified ACPI Table Header information:\n");
  PrintAcpiTableHeader(Table);
  Print(L"\nCheck checksum");
  UINT8 Checksum;
  Checksum = CalculateChecksum ((UINT8*)Table, Table->Length);
  if (Checksum != 0) {
    Print(L"Checksum error: 0x%02x\n", Checksum);
  } else {
    Print(L"Checksum is valid.\n");
  }
  return EFI_SUCCESS;
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
  Print (L"\nInitial ACPI Tables Information:\n");
  Print (L"=======================\n\n");
  
  PrintAllACPITables();
  
  Print (L"\nModifying ACPI Table...\n");

  EFI_ACPI_DESCRIPTION_HEADER NewHeader;
  SetMem(&NewHeader, sizeof(EFI_ACPI_DESCRIPTION_HEADER), 0); // 先清零
  
  /*
  typedef struct {
    UINT32  Signature;      // 表标识（如 'DSDT', 'SSDT' 等）
    UINT32  Length;         // 表总长度（字节）
    UINT8   Revision;       // 版本号
    UINT8   Checksum;       // 校验和
    UINT8   OemId[6];       // OEM ID
    UINT8   OemTableId[8];  // OEM 表ID
    UINT32  OemRevision;    // OEM 修订号
    UINT32  CreatorId;      // 创建者ID
    UINT32  CreatorRevision;// 创建者修订号
  } EFI_ACPI_DESCRIPTION_HEADER;
  */

  NewHeader.Signature        = SIGNATURE_32('F','A','C','P');
  NewHeader.Length           = 244; 
  NewHeader.Revision         = 1;
  NewHeader.Checksum         = 0;
  CopyMem(NewHeader.OemId,      "MYOEM ", 6);
  CopyMem(&NewHeader.OemTableId, "MYTABLE1", 8);
  NewHeader.OemRevision      = 0x20250613;
  NewHeader.CreatorId        = SIGNATURE_32('A','B','C','D');
  NewHeader.CreatorRevision  = 0x00010001;
  NewHeader.Checksum = (UINT8)(0 - CalculateChecksum((UINT8*)&NewHeader, NewHeader.Length));

  ChangeACPITable(2, &NewHeader);
  
  return EFI_SUCCESS;
}
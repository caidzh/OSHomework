#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <IndustryStandard/Acpi.h>
#include <Protocol/AcpiTable.h>

#define HARDWARE_RUNTIME_INFO_SIGNATURE SIGNATURE_32('H','W','I','N')

typedef struct {
    EFI_ACPI_DESCRIPTION_HEADER Header;
    UINT32 CpuTemperature;
    UINT32 Voltage;
    UINT32 Secret;
} HARDWARE_RUNTIME_INFO_TABLE;

EFI_STATUS
EFIAPI
HwinInstallAcpiTable (
  VOID
  )
{
  HARDWARE_RUNTIME_INFO_TABLE *Table;
  Table = AllocateZeroPool(sizeof(HARDWARE_RUNTIME_INFO_TABLE));

  Table->Header.Signature = HARDWARE_RUNTIME_INFO_SIGNATURE;
  Table->Header.Length = sizeof(HARDWARE_RUNTIME_INFO_TABLE);
  Table->CpuTemperature = 114514;
  Table->Voltage = 1919810;
  Table->Secret = 19260817;

  EFI_ACPI_TABLE_PROTOCOL *AcpiTableProtocol;
  UINTN TableKey;
  gBS->LocateProtocol(&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTableProtocol);
  AcpiTableProtocol->InstallAcpiTable(AcpiTableProtocol, Table, sizeof (HARDWARE_RUNTIME_INFO_TABLE), &TableKey);
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE ImageHandle,
  IN EFI_SYSTEM_TABLE *SystemTable
  )
{
  return HwinInstallAcpiTable();
}
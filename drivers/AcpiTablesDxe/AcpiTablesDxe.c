/** @file
  Install the SM8650 (Lanai) ACPI tables directly from the firmware image.

  AcpiPlatformDxe reads the tables back out of a firmware volume and crashes
  on this platform, so the tables are linked into this driver instead and
  handed straight to the ACPI table protocol.

  Copyright (c) 2026, Project Silicium contributors.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Uefi.h>

#include <IndustryStandard/Acpi.h>

#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Protocol/AcpiTable.h>

//
// Tables come from Silicium-ACPI/Platforms/OnePlus/giulia/AcpiTables.inf and
// are embedded as raw sections next to this driver (AcpiTablesData.c).
//
extern UINT8  gAcpiDsdt[];
extern UINT8  gAcpiApic[];
extern UINT8  gAcpiFadt[];
extern UINT8  gAcpiGtdt[];

typedef struct {
  CHAR8    *Name;
  UINT8    *Data;
} ACPI_TABLE_ENTRY;

STATIC ACPI_TABLE_ENTRY  mTables[] = {
  { "DSDT", gAcpiDsdt },
  { "APIC", gAcpiApic },
  { "FACP", gAcpiFadt },
  { "GTDT", gAcpiGtdt },
};

/**
  Recompute the ACPI table checksum in place.
**/
STATIC
VOID
FixTableChecksum (
  IN EFI_ACPI_DESCRIPTION_HEADER  *Header
  )
{
  UINT8  *Bytes;
  UINT8  Sum;
  UINTN  Index;

  Bytes                         = (UINT8 *)Header;
  Sum                           = 0;
  Bytes[OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER, Checksum)] = 0;

  for (Index = 0; Index < Header->Length; Index++) {
    Sum = (UINT8)(Sum + Bytes[Index]);
  }

  Bytes[OFFSET_OF (EFI_ACPI_DESCRIPTION_HEADER, Checksum)] = (UINT8)(0x100 - Sum);
}

EFI_STATUS
EFIAPI
AcpiTablesDxeEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                 Status;
  EFI_ACPI_TABLE_PROTOCOL    *AcpiTable;
  EFI_ACPI_DESCRIPTION_HEADER *Header;
  UINTN                      Handle;
  UINTN                      Index;
  UINTN                      Installed;

  Status = gBS->LocateProtocol (&gEfiAcpiTableProtocolGuid, NULL, (VOID **)&AcpiTable);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "AcpiTablesDxe: no ACPI table protocol (%r)\n", Status));
    return Status;
  }

  Installed = 0;

  for (Index = 0; Index < ARRAY_SIZE (mTables); Index++) {
    Header = (EFI_ACPI_DESCRIPTION_HEADER *)mTables[Index].Data;

    //
    // A zero length means the section was not linked in; skip it instead of
    // tripping over a bogus header.
    //
    if ((Header == NULL) || (Header->Length < sizeof (EFI_ACPI_DESCRIPTION_HEADER))) {
      DEBUG ((DEBUG_WARN, "AcpiTablesDxe: %a not linked, skipped\n", mTables[Index].Name));
      continue;
    }

    FixTableChecksum (Header);

    Handle = 0;
    Status = AcpiTable->InstallAcpiTable (AcpiTable, Header, Header->Length, &Handle);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "AcpiTablesDxe: %a install failed (%r)\n", mTables[Index].Name, Status));
      continue;
    }

    DEBUG ((DEBUG_INFO, "AcpiTablesDxe: %a installed, %d bytes\n", mTables[Index].Name, Header->Length));
    Installed++;
  }

  DEBUG ((DEBUG_INFO, "AcpiTablesDxe: %d table(s) installed\n", Installed));

  //
  // The tables live in the firmware image, the driver can go away.
  //
  return EFI_SUCCESS;
}

/** @file
  Install the bootloader-provided device tree as the EFI gFdtTableGuid
  configuration table, using the FDT HOB built by the SEC phase.

  The device tree handed over by ABL carries the per-boot Gunyah Resource
  Manager message-queue capability IDs; without forwarding it to the OS the
  mainline Gunyah driver cannot talk to the RM.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/HobLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>

#include <Guid/Fdt.h>
#include <Guid/FdtHob.h>

EFI_STATUS
EFIAPI
FdtDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  VOID        *Hob;
  VOID        *Fdt;
  UINT8       *Magic;
  EFI_STATUS  Status;

  Hob = GetFirstGuidHob (&gFdtHobGuid);
  if ((Hob == NULL) || (GET_GUID_HOB_DATA_SIZE (Hob) != sizeof (UINT64))) {
    DEBUG ((DEBUG_WARN, "FdtDxe: no FDT HOB found\n"));
    return EFI_NOT_FOUND;
  }

  Fdt   = (VOID *)(UINTN)*(UINT64 *)GET_GUID_HOB_DATA (Hob);
  Magic = (UINT8 *)Fdt;

  //
  // Flat device tree magic 0xd00dfeed, stored big-endian.
  //
  if ((Fdt == NULL) ||
      (Magic[0] != 0xD0) || (Magic[1] != 0x0D) ||
      (Magic[2] != 0xFE) || (Magic[3] != 0xED)) {
    DEBUG ((DEBUG_ERROR, "FdtDxe: invalid device tree @ 0x%p\n", Fdt));
    return EFI_NOT_FOUND;
  }

  Status = gBS->InstallConfigurationTable (&gFdtTableGuid, Fdt);
  DEBUG ((DEBUG_INFO, "FdtDxe: gFdtTableGuid @ 0x%p (%r)\n", Fdt, Status));
  return Status;
}

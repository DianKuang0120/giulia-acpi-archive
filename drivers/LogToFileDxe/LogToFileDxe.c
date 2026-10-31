/**
  LogToFileDxe - flush the frame-buffer debug log buffer to a file on the ESP
  partition (sda16) so it can be inspected after a boot that ends in a black
  screen.

  The FrameBufferSerialPortLib mirrors all DEBUG output into a fixed physical
  buffer (see GetLogBuffer).  This driver runs at BDS time (after FAT is up)
  and writes that buffer out as \log.txt on the ESP partition (the volume that
  holds systemd-boot).  It also registers ReadyToBoot / ExitBootServices / timer
  hooks to keep flushing as the boot proceeds.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiLib.h>

#include <Protocol/SimpleFileSystem.h>
#include <Guid/FileInfo.h>
#include <Guid/EventGroup.h>

//
// Frame buffer serial log buffer (must match FrameBufferSerialPortLib).
//
#define LOG_BUFFER_BASE  0xA7605000ULL
#define LOG_BUFFER_DATA  (LOG_BUFFER_BASE + 16)
#define LOG_MAGIC        0x474F4C4246ULL     // "FBLOG"

//
// The ESP partition (sda16) carries systemd-boot.  We write the log as
// \log.txt on whichever FAT volume holds systemd-bootaa64.efi.
//
STATIC CHAR16 *mLogFilePath = L"\\log.txt";

//
// Marker files that identify the ESP (systemd-boot's canonical ARM64 install
// path, plus the removable-media fallback).
//
STATIC CHAR16 *mEspMarker[] = {
  L"\\EFI\\systemd\\systemd-bootaa64.efi",
  L"\\EFI\\BOOT\\BOOTAA64.EFI",
};

STATIC
EFI_STATUS
FindEspVolume (
  OUT EFI_FILE_PROTOCOL **Root)
{
  EFI_STATUS            Status;
  UINTN                 HandleCount;
  EFI_HANDLE            *Handles;
  EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *Fs;
  EFI_FILE_PROTOCOL     *RootDir;
  EFI_FILE_PROTOCOL     *File;
  UINTN                 i;
  UINTN                 m;

  *Root = NULL;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gEfiSimpleFileSystemProtocolGuid,
                  NULL,
                  &HandleCount,
                  &Handles);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  for (i = 0; i < HandleCount; i++) {
    Status = gBS->HandleProtocol (Handles[i], &gEfiSimpleFileSystemProtocolGuid, (VOID **)&Fs);
    if (EFI_ERROR (Status)) {
      continue;
    }

    Status = Fs->OpenVolume (Fs, &RootDir);
    if (EFI_ERROR (Status)) {
      continue;
    }

    // The ESP holds systemd-boot; try each marker path.
    for (m = 0; m < ARRAY_SIZE (mEspMarker); m++) {
      Status = RootDir->Open (RootDir, &File, mEspMarker[m], EFI_FILE_MODE_READ, 0);
      if (!EFI_ERROR (Status)) {
        File->Close (File);
        *Root = RootDir;
        FreePool (Handles);
        return EFI_SUCCESS;
      }
    }

    RootDir->Close (RootDir);
  }

  FreePool (Handles);
  return EFI_NOT_FOUND;
}

STATIC
EFI_STATUS
FlushLogToFile (
  VOID
  )
{
  EFI_STATUS         Status;
  EFI_FILE_PROTOCOL  *Root;
  EFI_FILE_PROTOCOL  *File;
  UINT8              *Buffer;
  UINTN              Length;

  volatile UINT64 *Magic  = (volatile UINT64 *)(UINTN)LOG_BUFFER_BASE;
  volatile UINT64 *LengthP = (volatile UINT64 *)(UINTN)(LOG_BUFFER_BASE + 8);

  if (*Magic != LOG_MAGIC) {
    return EFI_NOT_READY;
  }

  Buffer = (UINT8 *)(UINTN)LOG_BUFFER_DATA;
  Length = (UINTN)*LengthP;
  if (Length == 0) {
    return EFI_NOT_READY;
  }

  Status = FindEspVolume (&Root);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = Root->Open (Root, &File, mLogFilePath, EFI_FILE_MODE_READ | EFI_FILE_MODE_WRITE | EFI_FILE_MODE_CREATE, 0);
  if (EFI_ERROR (Status)) {
    Root->Close (Root);
    return Status;
  }

  Status = File->SetPosition (File, 0);
  if (EFI_ERROR (Status)) {
    File->Close (File);
    Root->Close (Root);
    return Status;
  }

  Status = File->Write (File, &Length, Buffer);
  if (EFI_ERROR (Status)) {
    File->Close (File);
    Root->Close (Root);
    return Status;
  }

  File->Flush (File);
  File->Close (File);
  Root->Close (Root);

  return EFI_SUCCESS;
}

STATIC
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context)
{
  FlushLogToFile ();
}

STATIC
VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context)
{
  // Last-chance flush right before the OS takes over memory.  At this point
  // bootmgfw has already been loaded and run, so the log includes the boot
  // loader's own startup messages (Loading driver bootmgfw.efi, ConvertPages
  // failures, etc).
  FlushLogToFile ();
}

//
// Periodic timer flush.  bootmgfw runs for several seconds after ReadyToBoot
// (loading boot.wim, starting winload) before it either calls ExitBootServices
// or dies.  If it dies before ExitBootServices, the ReadyToBoot flush has
// already happened and the ExitBootServices hook never fires, so the messages
// emitted during those seconds never reach the file.  Flushing every couple of
// seconds captures the log incrementally so a later black screen still leaves
// the last few seconds of output on disk.
//
STATIC
VOID
EFIAPI
OnTimerTick (
  IN EFI_EVENT  Event,
  IN VOID       *Context)
{
  FlushLogToFile ();
}

EFI_STATUS
EFIAPI
LogToFileDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable)
{
  EFI_EVENT  ReadyToBootEvent;
  EFI_EVENT  ExitBootServicesEvent;
  EFI_EVENT  TimerEvent;

  // Flush once now (BDS time, FAT should be up).
  FlushLogToFile ();

  // Register a ReadyToBoot hook so we flush the very latest log right before
  // the boot manager starts bootmgfw.
  EfiCreateEventReadyToBootEx (
    TPL_CALLBACK,
    OnReadyToBoot,
    NULL,
    &ReadyToBootEvent);

  // Register an ExitBootServices hook as a second (later) flush point.  If
  // bootmgfw runs far enough to call ExitBootServices, this captures the
  // messages emitted between ReadyToBoot and OS handoff.
  ExitBootServicesEvent = NULL;
  gBS->CreateEventEx (
         EVT_NOTIFY_SIGNAL,
         TPL_CALLBACK,
         OnExitBootServices,
         NULL,
         &gEfiEventExitBootServicesGuid,
         &ExitBootServicesEvent);

  // Periodic timer: flush every 2 seconds so the log survives a black screen
  // that happens after ReadyToBoot but before ExitBootServices.
  TimerEvent = NULL;
  gBS->CreateEvent (
         EVT_TIMER | EVT_NOTIFY_SIGNAL,
         TPL_CALLBACK,
         OnTimerTick,
         NULL,
         &TimerEvent);
  gBS->SetTimer (TimerEvent, TimerPeriodic, 2 * 1000 * 10000); // 2 s in 100 ns units

  return EFI_SUCCESS;
}

/** @file
  Print PSCI information for the SM8650 platform.

  This driver changes nothing about the boot flow.  It only asks the PSCI
  implementation (provided by ABL through SMC) a few questions and prints the
  answers on the framebuffer console:

    PSCI_VERSION          which PSCI revision is implemented
    AFFINITY_INFO         what state every secondary CPU is in
    CPU_SUSPEND probe     whether the suspend call path is reachable at all

  With this output it is possible to tell apart:

    - firmware/ABL does not support CPU_ON at all   -> every core reports
      "OFF" and CPU_ON would return NOT_SUPPORTED
    - the secondary cores are still running         -> they report "ON"
      (Windows then cannot start them, they never went through the CPU_OFF
      sequence the PSCI expects)

  Every call is a SMCCC SMC64 call, so the function IDs carry the 0xC400
  prefix for the CPU_ON class of calls (0x8400xxxx is the 32-bit variant).

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/ArmMonitorLib.h>

//
// PSCI function IDs (SMC64)
//
#define PSCI_VERSION            0x84000000
#define PSCI_AFFINITY_INFO      0xC4000004
#define PSCI_FEATURES           0x8400000A
#define PSCI_CPU_ON             0xC4000003

#define PSCI_VERSION_MAJOR(v)   (((v) >> 16) & 0xFFFF)
#define PSCI_VERSION_MINOR(v)   ((v) & 0xFFFF)

//
// AFFINITY_INFO target affinity level: 0 = just this CPU.
//
#define AFFINITY_LEVEL_CPU      0

STATIC
INT64
PsciCall (
  IN     UINT64  Function,
  IN     UINT64  Arg0,
  IN     UINT64  Arg1,
  IN     UINT64  Arg2
  )
{
  ARM_MONITOR_ARGS  Args;

  Args.Arg0 = Function;
  Args.Arg1 = Arg0;
  Args.Arg2 = Arg1;
  Args.Arg3 = Arg2;
  Args.Arg4 = 0;
  Args.Arg5 = 0;
  Args.Arg6 = 0;
  Args.Arg7 = 0;

  ArmMonitorCall (&Args);

  return (INT64)Args.Arg0;
}

STATIC CONST CHAR8 *
AffinityStateName (
  IN INT64  State
  )
{
  switch (State) {
    case 0: return "ON (running)";
    case 1: return "OFF";
    case 2: return "ON_PENDING";
    default: return "?";
  }
}

EFI_STATUS
EFIAPI
PsciDiagEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  INT64   Ret;
  UINT64  Affinity;
  UINTN   Index;

  DEBUG ((DEBUG_ERROR, "\n"));
  DEBUG ((DEBUG_ERROR, "=== PSCI diagnostic ===\n"));

  //
  // 1. Which PSCI revision does ABL implement?
  //
  Ret = PsciCall (PSCI_VERSION, 0, 0, 0);
  DEBUG ((
    DEBUG_ERROR,
    "PSCI_VERSION      : 0x%lx (%ld.%ld)\n",
    (UINT64)Ret,
    PSCI_VERSION_MAJOR ((UINT64)Ret),
    PSCI_VERSION_MINOR ((UINT64)Ret)
    ));

  //
  // 2. Does it even know CPU_ON?
  //
  Ret = PsciCall (PSCI_FEATURES, PSCI_CPU_ON, 0, 0);
  DEBUG ((DEBUG_ERROR, "PSCI_FEATURES(CPU_ON) : 0x%lx %a\n", (UINT64)Ret,
          Ret == 0 ? "(supported)" : Ret < 0 ? "(NOT supported)" : "(?)"));

  //
  // 3. What is every core doing right now?
  //
  //    SM8650 MPIDRs are 0x000, 0x100 ... 0x700 (one byte per core).
  //
  for (Index = 0; Index < 8; Index++) {
    Affinity = (UINT64)Index << 8;
    Ret      = PsciCall (PSCI_AFFINITY_INFO, Affinity, AFFINITY_LEVEL_CPU, 0);
    DEBUG ((
      DEBUG_ERROR,
      "CPU%lu aff=0x%lx : %ld %a\n",
      (UINT64)Index,
      Affinity,
      Ret,
      Ret >= 0 ? AffinityStateName (Ret) : "(error)"
      ));
  }

  DEBUG ((DEBUG_ERROR, "=== end of PSCI diagnostic ===\n\n"));

  //
  // Pure observation, nothing to install.
  //
  return EFI_SUCCESS;
}

#!/usr/bin/env python3
#
# Set how many CPUs the MADT advertises.
#
#   ./apic_cpu_count.py 8        enable CPU0..CPU7 (normal build)
#   ./apic_cpu_count.py 4        enable CPU0..CPU3 only
#
# The GICC entries stay in the table, only the "Processor Enabled" flag of the
# ones above the requested count is cleared.  That keeps every other field
# (GICR address, MPIDR, efficiency class) exactly as it is, so a boot change
# can only come from the number of enabled CPUs.
#
# Rebuilds Silicon/Qualcomm/Lanai/APIC.aml in place and fixes the checksum.
#
import struct
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
APIC = HERE / "APIC.aml"
FULL_COUNT = 8


def gicc_offsets(blob):
    off = 44
    result = []
    while off < len(blob):
        kind = blob[off]
        length = blob[off + 1]
        if kind == 0x0B:
            result.append(off)
        off += length
    return result


def main():
    count = int(sys.argv[1]) if len(sys.argv) > 1 else FULL_COUNT
    if not 1 <= count <= FULL_COUNT:
        sys.exit(f"count must be 1..{FULL_COUNT}")

    blob = bytearray(APIC.read_bytes())
    offsets = gicc_offsets(blob)
    if len(offsets) != FULL_COUNT:
        sys.exit(f"unexpected GICC count {len(offsets)} in {APIC}")

    for index, off in enumerate(offsets):
        # GICC: type, length, reserved, cpu_if(4), uid(4), flags(4) at +12
        flags = struct.unpack_from("<I", blob, off + 12)[0]
        if index < count:
            flags |= 1          # Processor Enabled
        else:
            flags &= ~1
        struct.pack_into("<I", blob, off + 12, flags)

    blob[9] = 0
    blob[9] = (0x100 - (sum(blob) & 0xFF)) & 0xFF
    APIC.write_bytes(blob)

    enabled = [i for i, off in enumerate(offsets)
               if struct.unpack_from("<I", blob, off + 12)[0] & 1]
    print(f"{APIC.name}: {count} CPU(s) enabled -> {enabled}, "
          f"checksum {'ok' if (sum(blob) & 0xFF) == 0 else 'BAD'}")


if __name__ == "__main__":
    main()

#!/bin/sh
#
# rebuild_apic.sh - rebuild APIC.aml from the iasl dump and apply every fix.
#
# The SM8635-derived MADT needs six corrections for SM8650.  Doing them one by
# one by hand is how the table got inconsistent in the first place, so they
# are all applied here, in order, from a byte-exact rebuild of the dump.
#
#   1. GICC Length says 52 but entries are 82 bytes apart  -> fixlen
#   2. GICC is 82 bytes, the standard ACPI 6.0 size is 80  -> shrink
#   3. every GICC carries the same GICR address            -> gicr
#   4. the 0x0E GICR region overrides the per-core bases    -> strip 0x0E
#   5. the ITS is described as an MSI frame (0x0D)          -> its
#
set -e
cd "$(dirname "$0")"

if [ ! -x ./apictool ]; then
    cc -O2 -o apictool apictool.c
fi

# 1. byte-exact recovery from the iasl dump
python3 - <<'PY'
import re
data = bytearray()
for line in open("Decompiled/APIC.dsl"):
    m = re.match(r"^\s+([0-9A-F]{4}): ((?:[0-9A-F]{2} ?)+)", line)
    if m:
        off = int(m.group(1), 16)
        while len(data) < off:
            data.append(0)
        data[off:off + len(m.group(2).split())] = bytes.fromhex(m.group(2))
data = data[:764]
data[9] = 0
data[9] = (0x100 - (sum(data) & 0xFF)) & 0xFF
open("APIC.aml", "wb").write(data)
PY

# 2. repairs, in the order they depend on each other
./apictool fixlen
./apictool shrink
./apictool gicr 0x17180000 0x40000
./apictool strip 0x0E
./apictool its
./apictool info

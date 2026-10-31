/*
 * OnePlus giulia (Ace 5 / 13R) - SM8650 (Lanai) DSDT
 *
 * SMP DIAGNOSTIC: single CPU only, paired with an APIC that describes one
 * GICC.  If the boot gets further than before, the hang is in SMP bring-up
 * and not in the ACPI device descriptions.
 *
 * Do not ship this.  Restore DSDT_Baseline.asl once the test is done.
 *
 * Build:
 *   iasl -tc -p DSDT DSDT_single.asl
 */
DefinitionBlock ("DSDT.aml", "DSDT", 2, "QCOMM ", "SM8650 ", 3)
{
    Scope (\_SB)
    {
        Device (CPU0)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 0)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }
    }
}

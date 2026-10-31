/*
 * OnePlus giulia (Ace 5 / 13R) - SM8650 (Lanai) DSDT
 *
 * BASELINE version: this is the configuration that is known to reach the
 * WinPE boot manager (spinning dots) on hardware.  Do not add devices here
 * without testing on device - a previous attempt with UFS/PCIe/SMMU/GPIO
 * nodes made the boot stop before the spinner.
 *
 * The fuller version with UFS0 / PCI0 / PCI1 / SMMU / GIO0 / UART is in
 * DSDT.asl, which is not compiled into the image right now.
 *
 * Build with:
 *   iasl -tc DSDT_Baseline.asl -p DSDT.aml
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

        Device (CPU1)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 1)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU2)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 2)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU3)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 3)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU4)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 4)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU5)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 5)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU6)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 6)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }

        Device (CPU7)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 7)
            Method (_STA, 0, NotSerialized) { Return (0x0F) }
        }
    }
}

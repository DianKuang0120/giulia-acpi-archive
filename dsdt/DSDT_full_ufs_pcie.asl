/*
 * OnePlus giulia (Ace 5 / 13R) - SM8650 (Lanai) DSDT
 *
 * This is the hand-maintained source of DSDT.aml.  Build it with:
 *
 *   iasl -tc DSDT.asl
 *
 * Addresses come from the upstream device tree
 * (arch/arm64/boot/dts/qcom/sm8650.dtsi, sm8650-oneplus-giulia.dts).
 *
 * Only devices that are actually reachable by Windows are described here:
 *
 *   CPU0..CPU7  ACPI0007 processor containers (paired with the MADT)
 *   UFS0        QCOM24A5, the internal UFS 2.x/3.x controller
 *   SDC0        QCOM22D1, the (unused) SD card controller - kept for the
 *               Windows SD host stack so it does not probe a phantom device
 *   PCI0/PCI1   the two PCIe root complexes (PNP0A08/PNP0A03)
 *   SMMU        ARM SMMU-500 (PNP0C02 "system resource") so the HAL can see
 *               the IOMMU even without IORT
 *   GIO0        TLMM pin controller (QCOM2501)
 *   UART        debug UART (QCOM1A16), also referenced by SPCR/DBG2
 *
 * Devices deliberately *not* described yet:
 *   SPMI/PMIC, thermal zones, battery, touchscreen, charging - they need
 *   per-device _DSM/_CRS work and are not required to reach WinPE.
 *
 * _STA policy: return 0x0F for devices that do not have _DEP or power
 * resources (Windows expects the "present + enabled + functioning" bits with
 * bit 2 cleared, which is 0x0B; 0x0F only for containers that are always on).
 */
DefinitionBlock ("DSDT.aml", "DSDT", 2, "QCOM  ", "SM8650  ", 0x00008650)
{
    Scope (\_SB)
    {
        /*
         * ------------------------------------------------------------------
         * Processors
         * ------------------------------------------------------------------
         */
        Device (CPU0)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 0)
            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        Device (CPU1)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 1)
            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        Device (CPU2)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 2)
            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        Device (CPU3)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 3)
            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        Device (CPU4)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 4)
            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        Device (CPU5)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 5)
            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        Device (CPU6)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 6)
            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        Device (CPU7)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 7)
            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        /*
         * ------------------------------------------------------------------
         * UFS host controller
         *
         * sm8650.dtsi: ufshc@1d84000, SPI 265 (== GSI 0xE9 after the
         * "SPI - 136" left shift Windows applies on ARM64).
         * The child DEV0 with _ADR 0x10 is the UFS device well known LUN.
         * ------------------------------------------------------------------
         */
        Device (UFS0)
        {
            Name (_HID, "QCOM24A5")
            Name (_UID, Zero)
            Name (_CCA, One)

            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x01D84000, 0x00015000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x000000E9 }
                })
                Return (RBUF)
            }

            Method (_STA, 0, NotSerialized) { Return (0x0B) }

            Device (DEV0)
            {
                Name (_ADR, 0x10)
                Method (_STA, 0, NotSerialized) { Return (0x0B) }
                Method (_RMV, 0, NotSerialized) { Return (Zero) }
            }
        }

        /*
         * ------------------------------------------------------------------
         * PCIe root complex 0  (Wi-Fi, segment 0)
         *
         * sm8650.dtsi: pcie@1c00000
         *   parf      0x01C00000
         *   ECAM      0x60100000, bus 0x00..0x0F   (see MCFG)
         *   bus window 0x60200000..0x602FFFFF (I/O)
         *              0x60300000..0x63FFFFFF (32-bit memory, non-prefetch)
         *   INTx      SPI 149..152 -> GSI 0x95..0x98
         *
         * _CBA is mandatory for PNP0A08 on ACPI 5.0+: Windows uses it to find
         * the ECAM window instead of relying on MCFG alone.
         * ------------------------------------------------------------------
         */
        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A08"))
            Name (_CID, EisaId ("PNP0A03"))
            Name (_UID, Zero)
            Name (_SEG, Zero)
            Name (_BBN, Zero)
            Name (_CCA, One)

            Method (_CBA, 0, NotSerialized) { Return (0x60100000) }

            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
                        0x0000,             // Granularity
                        0x0000,             // Address Minimum (Bus 0)
                        0x000F,             // Address Maximum (Bus 15)
                        0x0000,             // Address Translation
                        0x0010,             // Address Length
                        ,, )
                    Memory32Fixed (ReadWrite, 0x60100000, 0x00100000)  // ECAM
                    DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                        0x00000000,         // Granularity
                        0x00000000,         // Address Minimum
                        0x000FFFFF,         // Address Maximum
                        0x60200000,         // Address Translation -> 0x60200000
                        0x00100000,         // Address Length
                        ,, , TypeTranslation, )
                    DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                        0x00000000,         // Granularity
                        0x60300000,         // Address Minimum
                        0x63FFFFFF,         // Address Maximum
                        0x00000000,         // Address Translation
                        0x03D00000,         // Address Length
                        ,, , AddressRangeMemory, TypeStatic)
                })
                Return (RBUF)
            }

            Method (_PRT, 0, NotSerialized)  // PCI routing table
            {
                Return (Package (0x04)
                {
                    Package (0x04) { 0xFFFF, Zero,  Zero, 0x95 },
                    Package (0x04) { 0xFFFF, One,   Zero, 0x96 },
                    Package (0x04) { 0xFFFF, 0x02,  Zero, 0x97 },
                    Package (0x04) { 0xFFFF, 0x03,  Zero, 0x98 }
                })
            }

            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        /*
         * ------------------------------------------------------------------
         * PCIe root complex 1  (USB-C / dock, segment 1)
         *
         * sm8650.dtsi: pcie@1c08000
         *   parf      0x01C08000
         *   ECAM      0x40100000, bus 0x00..0xFF   (see MCFG)
         *   bus window 0x40200000..0x402FFFFF (I/O)
         *              0x40300000..0x43FFFFFF (32-bit memory, non-prefetch)
         *   INTx      SPI 434..439 -> GSI 0x1B2..  (only INTA/INTB/D/C used)
         * ------------------------------------------------------------------
         */
        Device (PCI1)
        {
            Name (_HID, EisaId ("PNP0A08"))
            Name (_CID, EisaId ("PNP0A03"))
            Name (_UID, One)
            Name (_SEG, One)
            Name (_BBN, Zero)
            Name (_CCA, One)

            Method (_CBA, 0, NotSerialized) { Return (0x40100000) }

            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    WordBusNumber (ResourceProducer, MinFixed, MaxFixed, PosDecode,
                        0x0000,             // Granularity
                        0x0000,             // Address Minimum
                        0x00FF,             // Address Maximum
                        0x0000,             // Address Translation
                        0x0100,             // Address Length
                        ,, )
                    Memory32Fixed (ReadWrite, 0x40100000, 0x00100000)  // ECAM
                    DWordIO (ResourceProducer, MinFixed, MaxFixed, PosDecode, EntireRange,
                        0x00000000,
                        0x00000000,
                        0x000FFFFF,
                        0x40200000,
                        0x00100000,
                        ,, , TypeTranslation, )
                    DWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed, NonCacheable, ReadWrite,
                        0x00000000,
                        0x40300000,
                        0x43FFFFFF,
                        0x00000000,
                        0x03D00000,
                        ,, , AddressRangeMemory, TypeStatic)
                })
                Return (RBUF)
            }

            /*
             * sm8650.dtsi: INTx = SPI 434 ISR-lines for INTA..INTD of PCI1.
             * GSI = SPI - 136 (ARM64), so 434 -> 0x12A.
             */
            Method (_PRT, 0, NotSerialized)
            {
                Return (Package (0x04)
                {
                    Package (0x04) { 0xFFFF, Zero,  Zero, 0x12A },
                    Package (0x04) { 0xFFFF, One,   Zero, 0x12B },
                    Package (0x04) { 0xFFFF, 0x02,  Zero, 0x12E },
                    Package (0x04) { 0xFFFF, 0x03,  Zero, 0x12F }
                })
            }

            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        /*
         * ------------------------------------------------------------------
         * Application SMMU-500
         *
         * sm8650.dtsi: iommu@15000000, GIC SPI 65.. => GSI 65 - 136 is
         * negative, the SMMU global interrupt is a PPI-less SPI line that is
         * described in IORT.  Here it is only exported as a BIOS-owned MMIO
         * range so that Windows HAL pokes the right registers when it builds
         * the page tables, but does not attach the device to a driver we do
         * not ship (hence DeviceClass = reserved).
         *
         * HID PNP0C02 = "Generic system resources"; the first descriptor is
         * a QWordSpace so >4 GiB ranges stay describable.
         * ------------------------------------------------------------------
         */
        Device (SMMU)
        {
            Name (_HID, EisaId ("PNP0C02"))
            Name (_UID, 0x08)
            Name (_CCA, One)

            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    QWordMemory (ResourceProducer, PosDecode, MinFixed, MaxFixed,
                        NonCacheable, ReadWrite,
                        0x00000000,         // Granularity
                        0x15000000,         // Range Minimum
                        0x15FFFFFF,         // Range Maximum
                        0x00000000,         // Translation Offset
                        0x01000000,         // Length
                        ,, , AddressRangeMemory, TypeStatic)
                })
                Return (RBUF)
            }

            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        /*
         * ------------------------------------------------------------------
         * TLMM GPIO controller
         *
         * sm8650.dtsi: pinctrl@f100000, 211 GPIOs, GIC SPI 208.
         * ------------------------------------------------------------------
         */
        Device (GIO0)
        {
            Name (_HID, "QCOM2501")
            Name (_UID, Zero)
            Name (_CCA, Zero)

            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x0F100000, 0x00300000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x000000D0 }
                })
                Return (RBUF)
            }

            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }

        /*
         * ------------------------------------------------------------------
         * Debug UART (SE7 at 0x089C0000, GIC SPI 462 -> GSI 0x1CE)
         *
         * Referenced by SPCR/DBG2 via the namespace name \_SB.UART.
         * ------------------------------------------------------------------
         */
        Device (UART)
        {
            Name (_HID, "QCOM1A16")
            Name (_UID, 0x0F)
            Name (_CCA, Zero)
            Name (_STR, Unicode ("QUP_2_SE_7,4W,DEBUG_UART"))

            Method (_CRS, 0, NotSerialized)
            {
                Name (RBUF, ResourceTemplate ()
                {
                    Memory32Fixed (ReadWrite, 0x0089C000, 0x00004000)
                    Interrupt (ResourceConsumer, Level, ActiveHigh, Exclusive) { 0x000001CE }
                })
                Return (RBUF)
            }

            Method (_STA, 0, NotSerialized) { Return (0x0B) }
        }
    }
}

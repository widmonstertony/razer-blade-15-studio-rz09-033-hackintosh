/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20260408 (32-bit version)
 * Copyright (c) 2000 - 2026 Intel Corporation
 *
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of EFI/OC/ACPI/SSDT-I2C.aml
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x0000019B (411)
 *     Revision         0x02
 *     Checksum         0xE9
 *     OEM ID           "hack"
 *     OEM Table ID     "HACK"
 *     OEM Revision     0x00000000 (0)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20200925 (538970405)
 */
DefinitionBlock ("", "SSDT", 2, "hack", "HACK", 0x00000000)
{
    External (_SB_.PCI0.I2C0, DeviceObj)
    External (FMD0, IntObj)
    External (FMH0, IntObj)
    External (FML0, IntObj)
    External (FPD0, IntObj)
    External (FPH0, IntObj)
    External (FPL0, IntObj)
    External (HMD0, IntObj)
    External (HMH0, IntObj)
    External (HML0, IntObj)
    External (M0C0, IntObj)
    External (M1C0, IntObj)
    External (SSD0, IntObj)
    External (SSH0, IntObj)
    External (SSL0, IntObj)

    Scope (_SB.PCI0.I2C0)
    {
        If (_OSI ("Darwin"))
        {
            Method (PKGY, 1, Serialized)
            {
                Name (PKG, Package (0x01)
                {
                    Zero
                })
                PKG [Zero] = Arg0
                Return (PKG) /* \_SB_.PCI0.I2C0.PKGY.PKG_ */
            }

            Method (PKGX, 3, Serialized)
            {
                Name (PKG, Package (0x03)
                {
                    Zero,
                    Zero,
                    Zero
                })
                PKG [Zero] = Arg0
                PKG [One] = Arg1
                PKG [0x02] = Arg2
                Return (PKG) /* \_SB_.PCI0.I2C0.PKGX.PKG_ */
            }

            Method (SSCN, 0, NotSerialized)
            {
                Return (PKGX (SSH0, SSL0, SSD0))
            }

            Method (FMCN, 0, NotSerialized)
            {
                Return (PKGX (FMH0, FML0, FMD0))
            }

            Method (FPCN, 0, NotSerialized)
            {
                Return (PKGX (FPH0, FPL0, FPD0))
            }

            Method (HMCN, 0, NotSerialized)
            {
                Return (PKGX (HMH0, HML0, HMD0))
            }

            Method (M0D3, 0, NotSerialized)
            {
                Return (PKGY (M0C0))
            }

            Method (M1D3, 0, NotSerialized)
            {
                Return (PKGY (M1C0))
            }
        }
    }
}

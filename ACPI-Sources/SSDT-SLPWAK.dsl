/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20260408 (32-bit version)
 * Copyright (c) 2000 - 2026 Intel Corporation
 *
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of EFI/OC/ACPI/SSDT-SLPWAK.aml
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x00000188 (392)
 *     Revision         0x02
 *     Checksum         0x1C
 *     OEM ID           "hack"
 *     OEM Table ID     "PTSWAK"
 *     OEM Revision     0x00000000 (0)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20180427 (538444839)
 */
DefinitionBlock ("", "SSDT", 2, "hack", "PTSWAK", 0x00000000)
{
    External (_SB_.LID0, DeviceObj)
    External (_SB_.LID_, DeviceObj)
    External (_SB_.PCI0.LPCB.LID0, DeviceObj)
    External (_SB_.PCI0.LPCB.LID_, DeviceObj)
    External (ZWAK, MethodObj)    // 1 Arguments

    Method (EXT4, 1, NotSerialized)
    {
        If ((0x03 == Arg0))
        {
            If (CondRefOf (\_SB.LID))
            {
                Notify (\_SB.LID, 0x80) // Status Change
            }

            If (CondRefOf (\_SB.LID0))
            {
                Notify (\_SB.LID0, 0x80) // Status Change
            }

            If (CondRefOf (\_SB.PCI0.LPCB.LID))
            {
                Notify (\_SB.PCI0.LPCB.LID, 0x80) // Status Change
            }

            If (CondRefOf (\_SB.PCI0.LPCB.LID0))
            {
                Notify (\_SB.PCI0.LPCB.LID0, 0x80) // Status Change
            }
        }
    }

    Scope (_SB)
    {
        Device (PCI9)
        {
            Name (_ADR, Zero)  // _ADR: Address
            Name (FNOK, Zero)
        }
    }

    Method (_WAK, 1, NotSerialized)  // _WAK: Wake
    {
        If (_OSI ("Darwin"))
        {
            If ((\_SB.PCI9.FNOK == One))
            {
                \_SB.PCI9.FNOK = Zero
                Arg0 = 0x03
            }

            If (CondRefOf (EXT4))
            {
                EXT4 (Arg0)
            }
        }

        Local0 = ZWAK (Arg0)
        Return (Local0)
    }
}

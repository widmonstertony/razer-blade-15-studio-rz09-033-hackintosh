/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20260408 (32-bit version)
 * Copyright (c) 2000 - 2026 Intel Corporation
 *
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of EFI/OC/ACPI/SSDT-BATT.aml
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x000006CD (1741)
 *     Revision         0x02
 *     Checksum         0x37
 *     OEM ID           "CORP"
 *     OEM Table ID     "BATT"
 *     OEM Revision     0x00000000 (0)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20180427 (538444839)
 */
DefinitionBlock ("", "SSDT", 2, "CORP", "BATT", 0x00000000)
{
    External (_SB_.PCI0.LPCB, DeviceObj)
    External (_SB_.PCI0.LPCB.AC0_, DeviceObj)
    External (_SB_.PCI0.LPCB.BAT0.BFB0, UnknownObj)
    External (_SB_.PCI0.LPCB.BAT0.PAK1, UnknownObj)
    External (_SB_.PCI0.LPCB.BAT0.XBIF, MethodObj)    // 0 Arguments
    External (_SB_.PCI0.LPCB.BAT0.XBST, MethodObj)    // 0 Arguments
    External (_SB_.PCI0.LPCB.EC0_, DeviceObj)
    External (_SB_.PCI0.LPCB.EC0_.ECON, UnknownObj)
    External (_SB_.PCI0.LPCB.EC0_.PSTA, IntObj)
    External (BFB0, IntObj)
    External (PAK1, IntObj)

    Scope (_SB.PCI0.LPCB.AC0)
    {
        If (_OSI ("Darwin"))
        {
            Name (_PRW, Package (0x02)  // _PRW: Power Resources for Wake
            {
                0x18,
                0x03
            })
        }
        Else
        {
        }
    }

    Scope (_SB.PCI0.LPCB.EC0)
    {
        OperationRegion (ERM2, EmbeddedControl, 0x90, 0x0A)
        Field (ERM2, ByteAcc, NoLock, Preserve)
        {
            IF00,   8,
            IF01,   8,
            IF10,   8,
            IF11,   8,
            IF20,   8,
            IF21,   8,
            IF30,   8,
            IF31,   8,
            IF40,   8,
            IF41,   8
        }

        OperationRegion (ERM3, EmbeddedControl, 0xA2, 0x08)
        Field (ERM3, ByteAcc, NoLock, Preserve)
        {
            ST00,   8,
            ST01,   8,
            ST10,   8,
            ST11,   8,
            ST20,   8,
            ST21,   8,
            ST30,   8,
            ST31,   8
        }
    }

    Method (\_SB.PCI0.LPCB.EC0.RE1B, 1, NotSerialized)
    {
        OperationRegion (ERAM, EmbeddedControl, Arg0, One)
        Field (ERAM, ByteAcc, NoLock, Preserve)
        {
            BYTE,   8
        }

        Return (BYTE) /* \_SB_.PCI0.LPCB.EC0_.RE1B.BYTE */
    }

    Method (\_SB.PCI0.LPCB.EC0.RECB, 2, Serialized)
    {
        Arg1 = ((Arg1 + 0x07) >> 0x03)
        Name (TEMP, Buffer (Arg1){})
        Arg1 += Arg0
        Local0 = Zero
        While ((Arg0 < Arg1))
        {
            TEMP [Local0] = RE1B (Arg0)
            Arg0++
            Local0++
        }

        Return (TEMP) /* \_SB_.PCI0.LPCB.EC0_.RECB.TEMP */
    }

    Method (\_SB.PCI0.LPCB.BAT0._BIF, 0, NotSerialized)  // _BIF: Battery Information
    {
        If (_OSI ("Darwin"))
        {
            If ((\_SB.PCI0.LPCB.EC0.ECON == One))
            {
                Local0 = (\_SB.PCI0.LPCB.EC0.PSTA & 0x02)
                If (Local0)
                {
                    Local0 = B1B2 (\_SB.PCI0.LPCB.EC0.IF00, \_SB.PCI0.LPCB.EC0.IF01)
                    PAK1 [Zero] = Local0
                    If (Local0)
                    {
                        PAK1 [One] = B1B2 (\_SB.PCI0.LPCB.EC0.IF10, \_SB.PCI0.LPCB.EC0.IF11)
                        PAK1 [0x02] = B1B2 (\_SB.PCI0.LPCB.EC0.IF20, \_SB.PCI0.LPCB.EC0.IF21)
                    }
                    Else
                    {
                        Local1 = (B1B2 (\_SB.PCI0.LPCB.EC0.IF10, \_SB.PCI0.LPCB.EC0.IF11) * 0x0A)
                        PAK1 [One] = Local1
                        Local1 = (B1B2 (\_SB.PCI0.LPCB.EC0.IF20, \_SB.PCI0.LPCB.EC0.IF21) * 0x0A)
                        PAK1 [0x02] = Local1
                    }

                    PAK1 [0x03] = B1B2 (\_SB.PCI0.LPCB.EC0.IF30, \_SB.PCI0.LPCB.EC0.IF31)
                    PAK1 [0x04] = B1B2 (\_SB.PCI0.LPCB.EC0.IF40, \_SB.PCI0.LPCB.EC0.IF41)
                    PAK1 [0x05] = (B1B2 (\_SB.PCI0.LPCB.EC0.IF10, \_SB.PCI0.LPCB.EC0.IF11) / 0x32)
                    PAK1 [0x06] = (B1B2 (\_SB.PCI0.LPCB.EC0.IF10, \_SB.PCI0.LPCB.EC0.IF11) / 0x64)
                    PAK1 [0x0A] = ToString (\_SB.PCI0.LPCB.EC0.RECB (0x60, 0x0100), 0x20)
                    Return (PAK1) /* External reference */
                }
                Else
                {
                    Return (PAK1) /* External reference */
                }
            }
            Else
            {
                Return (PAK1) /* External reference */
            }
        }
        Else
        {
            \_SB.PCI0.LPCB.BAT0.XBIF ()
        }
    }

    Method (\_SB.PCI0.LPCB.BAT0._BST, 0, NotSerialized)  // _BST: Battery Status
    {
        If (_OSI ("Darwin"))
        {
            If ((\_SB.PCI0.LPCB.EC0.ECON == One))
            {
                Local0 = (\_SB.PCI0.LPCB.EC0.PSTA & 0x02)
                If (Local0)
                {
                    BFB0 [Zero] = B1B2 (\_SB.PCI0.LPCB.EC0.ST00, \_SB.PCI0.LPCB.EC0.ST01)
                    BFB0 [One] = B1B2 (\_SB.PCI0.LPCB.EC0.ST10, \_SB.PCI0.LPCB.EC0.ST11)
                    BFB0 [0x02] = B1B2 (\_SB.PCI0.LPCB.EC0.ST20, \_SB.PCI0.LPCB.EC0.ST21)
                    BFB0 [0x03] = B1B2 (\_SB.PCI0.LPCB.EC0.ST30, \_SB.PCI0.LPCB.EC0.ST31)
                    Return (BFB0) /* External reference */
                }
                Else
                {
                    Return (BFB0) /* External reference */
                }
            }
            Else
            {
                Return (BFB0) /* External reference */
            }
        }
        Else
        {
            \_SB.PCI0.LPCB.BAT0.XBST ()
        }
    }

    Method (B1B2, 2, NotSerialized)
    {
        Return ((Arg0 | (Arg1 << 0x08)))
    }
}

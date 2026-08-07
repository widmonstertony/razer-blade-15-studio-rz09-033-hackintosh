/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20260408 (32-bit version)
 * Copyright (c) 2000 - 2026 Intel Corporation
 *
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of EFI/OC/ACPI/SSDT-TPDX.aml
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x0000007B (123)
 *     Revision         0x02
 *     Checksum         0xC1
 *     OEM ID           "ACDT"
 *     OEM Table ID     "I2C-TPXX"
 *     OEM Revision     0x00000000 (0)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20200925 (538970405)
 */
DefinitionBlock ("", "SSDT", 2, "ACDT", "I2C-TPXX", 0x00000000)
{
    External (_SB_.PCI0.I2C0, DeviceObj)
    External (_SB_.PCI0.I2C0.TPD0, DeviceObj)

    Scope (_SB.PCI0.I2C0)
    {
        Scope (TPD0)
        {
            If (_OSI ("Darwin"))
            {
                Name (OSYS, 0x07DF)
            }
        }
    }
}

/*
 * Intel ACPI Component Architecture
 * AML/ASL+ Disassembler version 20260408 (32-bit version)
 * Copyright (c) 2000 - 2026 Intel Corporation
 *
 * Disassembling to symbolic ASL+ operators
 *
 * Disassembly of EFI/OC/ACPI/SSDT-ALC298.aml
 *
 * Original Table Header:
 *     Signature        "SSDT"
 *     Length           0x00000186 (390)
 *     Revision         0x01
 *     Checksum         0x74
 *     OEM ID           "hack"
 *     OEM Table ID     "_ALC298"
 *     OEM Revision     0x00000000 (0)
 *     Compiler ID      "INTL"
 *     Compiler Version 0x20180427 (538444839)
 */
DefinitionBlock ("", "SSDT", 1, "hack", "_ALC298", 0x00000000)
{
    External (_SB_.PCI0.HDEF, DeviceObj)

    Name (_SB.PCI0.HDEF.RMCF, Package (0x02)
    {
        "CodecCommander",
        Package (0x0A)
        {
            "Custom Commands",
            Package (0x04)
            {
                Package (0x00){},
                Package (0x08)
                {
                    "Command",
                    Buffer (0x04)
                    {
                         0x01, 0x87, 0x07, 0x22                           // ..."
                    },

                    "On Init",
                    ">y",
                    "On Sleep",
                    ">n",
                    "On Wake",
                    ">y"
                },

                Package (0x08)
                {
                    "Command",
                    Buffer (0x04)
                    {
                         0x01, 0xA7, 0x07, 0x23                           // ...#
                    },

                    "On Init",
                    ">y",
                    "On Sleep",
                    ">n",
                    "On Wake",
                    ">y"
                },

                Package (0x08)
                {
                    "Command",
                    Buffer (0x04)
                    {
                         0x02, 0x17, 0x08, 0x83                           // ....
                    },

                    "On Init",
                    ">y",
                    "On Sleep",
                    ">n",
                    "On Wake",
                    ">y"
                }
            },

            "Perform Reset",
            ">n",
            "Perform Reset on External Wake",
            ">n",
            "Send Delay",
            0x0A,
            "Sleep Nodes",
            ">n"
        }
    })
}

/*
 * Razer Blade 15 Studio OLED V61-P5 bounded native lid-wake correction.
 *
 * The paired OpenCore patches rename the firmware methods as follows:
 *   \_WAK                              -> \ZWAK
 *   \_SB.PCI0.LPCB.EC0.LID0._LID      -> ...XLID
 *
 * P4 armed LWFG before ZWAK but depended on a future _LID evaluation to
 * clear it.  If ZWAK did not synchronously cause that evaluation, the flag
 * survived the wake and the next real lid-close notification was incorrectly
 * returned as open.
 *
 * P5 bounds the override to the synchronous ZWAK call.  Every Darwin _LID
 * evaluation during ZWAK returns open without modifying the EC field.  After
 * ZWAK returns, _WAK unconditionally clears LWFG, synchronizes LIDS to open,
 * and notifies the active EC0.LID0 once.  All later physical lid events call
 * XLID directly and remain firmware-controlled.
 *
 * ACPI patches are global in OpenCore.  On non-Darwin systems LWFG is never
 * armed and the proxy is equivalent to the original firmware XLID method.
 */
DefinitionBlock ("", "SSDT", 2, "OC107", "LIDWK5", 0x00000000)
{
    External (LIDS, FieldUnitObj)
    External (_SB_.PCI0.LPCB.EC0_.LID0, DeviceObj)
    External (_SB_.PCI0.LPCB.EC0_.LID0.XLID, MethodObj) // 0 Arguments
    External (ZWAK, MethodObj)                           // 1 Argument

    Name (LWFG, Zero)

    Scope (_SB)
    {
        Device (PCI9)
        {
            Name (_ADR, Zero)
            Name (FNOK, Zero)
        }
    }

    Method (_WAK, 1, NotSerialized)
    {
        If (_OSI ("Darwin"))
        {
            If ((\_SB.PCI9.FNOK == One))
            {
                \_SB.PCI9.FNOK = Zero
                Arg0 = 0x03
            }

            If ((Arg0 == 0x03))
            {
                LWFG = One
            }
        }

        Local0 = ZWAK (Arg0)

        If (_OSI ("Darwin"))
        {
            If ((Arg0 == 0x03))
            {
                LWFG = Zero
                LIDS = One
                Notify (\_SB.PCI0.LPCB.EC0.LID0, 0x80)
            }
            Else
            {
                LWFG = Zero
            }
        }

        Return (Local0)
    }

    Scope (_SB.PCI0.LPCB.EC0.LID0)
    {
        Method (_LID, 0, NotSerialized)
        {
            If (_OSI ("Darwin"))
            {
                If ((\LWFG == One))
                {
                    Return (One)
                }
            }

            Return (XLID ())
        }
    }
}

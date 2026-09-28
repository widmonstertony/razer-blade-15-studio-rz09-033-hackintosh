#include <Headers/plugin_start.hpp>
#include <Headers/kern_api.hpp>
#include <Headers/kern_iokit.hpp>
#include <Headers/kern_patcher.hpp>
#include <IOKit/IOLib.h>
#include <IOKit/IOLocks.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/pwr_mgt/RootDomain.h>

static const char *cflFramebufferPaths[] {
	"/System/Library/Extensions/AppleIntelCFLGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelCFLGraphicsFramebuffer"
};

static KernelPatcher::KextInfo cflFramebuffer {
	"com.apple.driver.AppleIntelCFLGraphicsFramebuffer",
	cflFramebufferPaths,
	arrsize(cflFramebufferPaths),
	{},
	{},
	KernelPatcher::KextInfo::Unloaded
};

static const char *lightUpEDPSymbol =
	"__ZN31AppleIntelFramebufferController10LightUpEDPEP21AppleIntelFramebufferP21AppleIntelDisplayPathPK29IODetailedTimingInformationV2";
static const char *enableClocksSymbol =
	"__ZN31AppleIntelFramebufferController12EnableClocksEP21AppleIntelFramebufferP21AppleIntelDisplayPath";
static const char *hwSetPanelPowerSymbol =
	"__ZN31AppleIntelFramebufferController15hwSetPanelPowerEj";
static const char *hwSetPanelPowerConfigSymbol =
	"__ZN31AppleIntelFramebufferController21hwSetPanelPowerConfigEj";
static const char *setDPPowerStateSymbol =
	"__ZN31AppleIntelFramebufferController15SetDPPowerStateEP21AppleIntelFramebufferhP21AppleIntelDisplayPath";
static const char *readAUXSymbol =
	"__ZN31AppleIntelFramebufferController7ReadAUXEP21AppleIntelFramebufferjtPvP21AppleIntelDisplayPath";
static const char *writeAUXSymbol =
	"__ZN31AppleIntelFramebufferController8WriteAUXEP21AppleIntelFramebufferjtPvP21AppleIntelDisplayPath";
static const char *configureBufferTranslationSymbol =
	"__ZN31AppleIntelFramebufferController26ConfigureBufferTranslationEP21AppleIntelFramebufferP21AppleIntelDisplayPath";
static const char *checkClockRecoverySymbol =
	"__ZN31AppleIntelFramebufferController18CheckClockRecoveryEhPNS_18LinkTrainingStatusE";
static const char *setupParamsSymbol =
	"__ZN31AppleIntelFramebufferController11SetupParamsEP21AppleIntelFramebufferP21AppleIntelDisplayPathPNS_10CRTCParamsEPK29IODetailedTimingInformationV2";
static const char *setPowerWellStateSymbol =
	"__ZN31AppleIntelFramebufferController17setPowerWellStateEP21AppleIntelFramebufferb";
static const char *framebufferDoSetPowerStateSymbol =
	"__ZN21AppleIntelFramebuffer15doSetPowerStateEj";
static const char *readRegister32Symbol =
	"__ZN31AppleIntelFramebufferController14ReadRegister32Em";
static const char *writeRegister32Symbol =
	"__ZN31AppleIntelFramebufferController15WriteRegister32Emj";

using LightUpEDP = uint32_t (*)(void *controller, void *framebuffer,
	void *displayPath, const void *timing);
using EnableClocks = void (*)(void *controller, void *framebuffer,
	void *displayPath);
using HwSetPanelPower = uint32_t (*)(void *controller, uint32_t state);
using HwSetPanelPowerConfig = void (*)(void *controller, uint32_t value);
using SetDPPowerState = uint32_t (*)(void *controller, void *framebuffer,
	uint8_t state, void *displayPath);
using ReadAUX = IOReturn (*)(void *controller, void *framebuffer,
	uint32_t address, uint16_t length, void *buffer, void *displayPath);
using WriteAUX = IOReturn (*)(void *controller, void *framebuffer,
	uint32_t address, uint16_t length, void *buffer, void *displayPath);
using ConfigureBufferTranslation = void (*)(void *controller,
	void *framebuffer, void *displayPath);
using CheckClockRecovery = uint32_t (*)(void *controller, uint8_t lane,
	void *linkTrainingStatus);
using SetupParams = void (*)(void *controller, void *framebuffer,
	void *displayPath, void *crtcParams, const void *timing);
using SetPowerWellState = void (*)(void *controller, void *framebuffer,
	bool enable);
using FramebufferDoSetPowerState = uint32_t (*)(void *framebuffer,
	uint32_t newState);
using ReadRegister32 = uint32_t (*)(void *controller, uint32_t address);
using WriteRegister32 = void (*)(void *controller, uint32_t address,
	uint32_t value);
static LightUpEDP originalLightUpEDP = nullptr;
static EnableClocks originalEnableClocks = nullptr;
static HwSetPanelPower originalHwSetPanelPower = nullptr;
static HwSetPanelPowerConfig originalHwSetPanelPowerConfig = nullptr;
static SetDPPowerState callSetDPPowerState = nullptr;
static ReadAUX callReadAUX = nullptr;
static WriteAUX callWriteAUX = nullptr;
static ConfigureBufferTranslation originalConfigureBufferTranslation = nullptr;
static CheckClockRecovery originalCheckClockRecovery = nullptr;
static SetupParams originalSetupParams = nullptr;
static SetPowerWellState originalSetPowerWellState = nullptr;
static FramebufferDoSetPowerState originalFramebufferDoSetPowerState = nullptr;
static ReadRegister32 callReadRegister32 = nullptr;
static WriteRegister32 callWriteRegister32 = nullptr;

// Razer Blade 15 Studio Edition 2020 (RZ09-0330Q) BIOS 1.06 VBT,
// Samsung SDC_56WR06 / EDID 4C83:A029.  These are the board-specific PCH
// panel power sequencer values programmed by the Razer firmware at cold boot.
// S3 can reset this display power domain without running the GOP again.
static constexpr uint32_t kPanelPowerStatus = 0x000C7200;
static constexpr uint32_t kPanelPowerControl = 0x000C7204;
static constexpr uint32_t kPanelPowerOnDelays = 0x000C7208;
static constexpr uint32_t kPanelPowerOffDelays = 0x000C720C;
static constexpr uint32_t kPanelPowerDivisor = 0x000C7210;
static constexpr uint32_t kRazerPanelPowerOnDelays = 0x025807D0;
static constexpr uint32_t kRazerPanelPowerOffDelays = 0x01F407D0;
static constexpr uint32_t kPanelPowerOn = 0x80000000;
static constexpr uint32_t kPanelPowerSequenceMask = 0x30000000;

// Windows 31.0.101.2141's HSWEDPDDIA hot-plug path programs both of these
// controls before it asks the embedded DP encoder to leave D3.  The first is
// the CPU display-engine Port-A detector; the second is the SPT/PCH detector
// feeding SDEISR.  Tahoe checks SDEISR bit 24 in hwSetPanelPower, but does not
// rebuild either detector after this Razer's true S3 cycle.  Preserve the
// Windows read/modify/write masks exactly: status fields are W1C, enable bits
// are set, and unrelated ports remain untouched.
static constexpr uint32_t kDigitalPortHotPlugControl = 0x00044030;
static constexpr uint32_t kSouthDisplayInterruptIdentity = 0x000C4000;
static constexpr uint32_t kSouthDisplayInterruptMask = 0x000C4004;
static constexpr uint32_t kSouthDisplayInterruptEnable = 0x000C400C;
static constexpr uint32_t kSPTHotPlugControl = 0x000C4030;
static constexpr uint32_t kShortHotPlugFilterCount = 0x000C4038;
static constexpr uint32_t kDigitalPortAHotPlugEnable = 0x00000010;
static constexpr uint32_t kDigitalPortAHotPlugStatusW1C = 0x00000003;
static constexpr uint32_t kSPTPortAHotPlugEnable = 0x10000000;
static constexpr uint32_t kSPTPortAHotPlugStatusW1C = 0x03000000;
static constexpr uint32_t kSPTHotPlugWindowsPreserveMask = 0xEFFCFCFC;
static constexpr uint32_t kSDEPortAHotPlugLive = 0x01000000;
static constexpr uint32_t kHPDReadyPollDelayMs = 20;
static constexpr uint32_t kHPDReadyPollCount = 100;
// Samsung's same-generation ATNA56WR18 operating sequence requires the sink
// to receive DPCD SET_POWER=D3 for at least 100 ms before VCC is removed, and
// requires a minimum 500 ms rail-off interval before VCC is applied again.
// ATNA56WR06 is the electrically compatible 2019 panel in this Razer and
// exposes the same Intel DPCD 0x600 power control path.
static constexpr uint32_t kSamsungD3ToRailOffDelayMs = 100;
static constexpr uint32_t kSamsungRailOffMinimumMs = 500;
static constexpr uint32_t kSamsungPostPanelOnAUXDelayMs = 300;
static constexpr uint32_t kSamsungPostD0SettleMs = 500;
static constexpr size_t kControllerPanelPowerOnDelaysOffset = 0x2C20;
static constexpr size_t kControllerPanelPowerOffDelaysOffset = 0x2C24;

// Cannon Point-H GPIO community 2 is exposed by this machine's GPI0 ACPI
// device as the physical range FD6B0000-FD6BFFFF.  Hardware reports the
// community PADBAR as 0x0600.  Razer's ACPI GPCH table places GPP_F at 0x09D0
// after GPP_K/H/E; F19/F20/F21 are the dedicated L_VDDEN/L_BKLTEN/L_BKLTCTL
// pins.  The mapping is used to prove whether S3
// leaves the physical panel-power pin in the same native mode as cold boot.
// A repair, when needed, is limited to L_VDDEN's four PMODE bits and is only
// attempted after the address, PADBAR, cold value, and pad lock are verified.
static constexpr IOPhysicalAddress kPchGpioCommunityPhysical = 0xFD6B0000ULL;
static constexpr IOByteCount kPchGpioCommunityLength = 0x10000;
static constexpr uint32_t kPchGpioRevision = 0x0000;
static constexpr uint32_t kPchGpioPadBar = 0x000C;
static constexpr uint32_t kExpectedPchCommunityPadBar = 0x0600;
static constexpr uint32_t kGppFPadBase = 0x09D0;
static constexpr uint32_t kGppFPadStride = 0x10;
static constexpr uint32_t kGppFLVddEnPad = 19;
static constexpr uint32_t kGppFLBkltEnPad = 20;
static constexpr uint32_t kGppFLBkltCtlPad = 21;
static constexpr uint32_t kGppFLVddEnDw0 =
	kGppFPadBase + kGppFLVddEnPad * kGppFPadStride;
static constexpr uint32_t kGppFLVddEnDw1 = kGppFLVddEnDw0 + 4;
static constexpr uint32_t kGppFLBkltEnDw0 =
	kGppFPadBase + kGppFLBkltEnPad * kGppFPadStride;
static constexpr uint32_t kGppFLBkltEnDw1 = kGppFLBkltEnDw0 + 4;
static constexpr uint32_t kGppFLBkltCtlDw0 =
	kGppFPadBase + kGppFLBkltCtlPad * kGppFPadStride;
static constexpr uint32_t kGppFLBkltCtlDw1 = kGppFLBkltCtlDw0 + 4;
// Cannon Lake PADCFGLOCK uses one config-lock and one TX-lock dword per GPP.
// GPP_F is register group 3 and L_VDDEN is bit 19 in that group.
static constexpr uint32_t kPchPadCfgLockBase = 0x80;
static constexpr uint32_t kGppFRegisterGroup = 3;
static constexpr uint32_t kGppFPadCfgLock =
	kPchPadCfgLockBase + kGppFRegisterGroup * 8;
static constexpr uint32_t kGppFPadCfgLockTx = kGppFPadCfgLock + 4;
static constexpr uint32_t kGppFLVddEnLockBit = 1U << kGppFLVddEnPad;
static constexpr uint32_t kPadCfg0PModeMask = 0x00003C00;

// Coffee Lake keeps the Gen9 DDI current-boost/balance-leg controls outside
// the per-port buffer-translation table.  The successful Linux 7.0 S3 capture
// from this exact machine proves that its VBT selects the normal KBL H/S DP
// table, whose nine translation dwords are byte-for-byte identical to Apple's
// cold-working table.  The strongest remaining source-proven candidate
// difference is dynamic I_boost: upstream i915 programs this register before
// every source voltage change, explicitly disabling boost for selectors
// 0/1/2/4/5/7 and selecting boost 1 for selectors 3/6/8.  Tahoe 25.2's three
// native training call sites do not update it; V57 restores the final cold
// selector's boost-1 state before training.  This is a testable candidate, not
// a claim that I_boost is already proven to be the root cause.
static constexpr uint32_t kDisplayIOBalanceLeg = 0x0006C00C;
static constexpr uint32_t kBalanceLegMaskA = 0x00000700;
static constexpr uint32_t kBalanceLegMaskE = 0x00700000;
static constexpr uint32_t kBalanceLegDisableA = 0x00800000;
static constexpr uint32_t kBalanceLegDisableE = 0x08000000;
static constexpr uint32_t kBalanceLegControlledAE =
	kBalanceLegMaskA | kBalanceLegMaskE |
	kBalanceLegDisableA | kBalanceLegDisableE;
// This exact panel's successful pre-S3 capture reports all controlled A/E
// fields clear. Treat that observed working state as a valid quiescent
// baseline; the selector-specific write below establishes the Linux state.
static constexpr uint32_t kBalanceLegQuiescentAE = 0x00000000;
static constexpr uint32_t kBalanceLegIboost1AE = 0x00100100;
static constexpr uint32_t kBalanceLegIboostDisabledAE = 0x08800000;
static constexpr uint8_t kLinuxKBLDPTrainingIBoost[] {
	0, 0, 0, 1, 0, 0, 1, 0, 1
};
static_assert(arrsize(kLinuxKBLDPTrainingIBoost) == 9,
	"KBL DP translation/I_boost table must have nine entries");

// Skylake/Kaby Lake/Coffee Lake Port A uses nine low/high buffer-translation
// pairs.  Apple selects one pair for each DP voltage/pre-emphasis request.
// The table is inside the S3-reset display domain, unlike an ordinary display
// blank.  Preserve the exact table from this machine instead of hard-coding a
// generic Intel table or guessing a panel voltage.
static constexpr uint32_t kDDIBufferTranslationA = 0x00064E00;
static constexpr uint32_t kDDIBufferTranslationPairCount = 9;
static constexpr uint32_t kDDIBufferTranslationDwordCount =
	kDDIBufferTranslationPairCount * 2;

// Read-only checkpoints used to distinguish source-PHY loss from panel/AUX
// failure.  Values are published on the kext IORegistry service so the test
// watchdog can save them even when the internal panel remains black.
static constexpr uint32_t kDDIBufferControlA = 0x00064000;
static constexpr uint32_t kDPTransportControlA = 0x00064040;
static constexpr uint32_t kDPTransportStatusA = 0x00064044;
static constexpr uint32_t kPortClockSelectA = 0x00046100;
static constexpr uint32_t kDPLLControl1 = 0x0006C058;
static constexpr uint32_t kDPLLControl2 = 0x0006C05C;
static constexpr uint32_t kDPLLStatus = 0x0006C060;
static constexpr uint32_t kDPLLControl2PortAClockOff = 0x00008000;
static constexpr uint32_t kDPLLControl2PortAOverride = 0x00000001;
static constexpr uint32_t kDPLL0Lock = 0x00000001;
static constexpr uint32_t kLCPLL1Control = 0x00046010;
static constexpr uint32_t kDDIBufferEnable = 0x80000000;
// Intel's Gen9 DDI enable sequence requires the source buffer to become active
// before the sink is told to sample training pattern 1.  Tahoe's CFL driver
// writes DDI_BUF_CTL_A and immediately performs the DPCD training-pattern AUX
// write.  A display-only wake survives because the PHY never fully powers
// down; this Razer's real S3 path does not.  Match the upstream Gen9 fixed
// activation wait at the one LinkTraining call site, not at the global MMIO
// writer used by WhateverGreen's backlight support.
static constexpr uint32_t kDDIBufferActivationSettleUs = 1000;
// The Samsung receiver advertises TRAINING_AUX_RD_INTERVAL=2 at DPCD 0x00E.
// Tahoe converts that to an 8 ms Phase-1 clock-recovery wait.  The successful
// Windows S3 trace from this exact machine instead spends about 16 ms between
// the Phase-1 lane-set write and the DPCD 0x202 status read, while retaining an
// 8 ms Phase-2 equalization wait.  V52 retargets only Tahoe's Phase-1 IOSleep
// call and extends 8 -> 16 ms on a committed S3 wake.
static constexpr uint32_t kApplePhase1ClockRecoveryDelayMs = 8;
static constexpr uint32_t kWindowsPhase1ClockRecoveryDelayMs = 16;
static constexpr uint8_t kSamsungTrainingAuxReadInterval = 0x02;
static constexpr uint32_t kDDIAFourLaneCapability = 0x00000010;
static constexpr uint32_t kDDIPortWidthMask = 0x0000000E;
static constexpr uint32_t kDDIPortWidthFour = 0x00000006;
static constexpr uint32_t kDDISetupWorkingFieldMask = 0x0F00001F;
static constexpr size_t kCRTCParamsDDIBufferControlOffset = 0x44;
static constexpr size_t kFramebufferIndexOffset = 0x1DC;
static constexpr size_t kFramebufferPowerStateOffset = 0xA1F8;
static constexpr uint32_t kFramebufferSleepState = 0;
static constexpr uint32_t kFramebufferWakeState = 2;
static constexpr size_t kDisplayPathTypeOffset = 0x28;
static constexpr size_t kDisplayPathPortOffset = 0x450;
static constexpr uint32_t kInternalFramebufferIndex = 0;
static constexpr uint32_t kEDPPortType = 3;
static constexpr uint8_t kDDIAPortIndex = 0;
static constexpr uint32_t kTransDDIFunctionControlA = 0x00060400;
static constexpr uint32_t kTransDDIFunctionControlEDP = 0x0006F400;
static constexpr uint32_t kChickenTransEDP = 0x000420CC;
static constexpr uint32_t kPchResetWarningOption = 0x00046408;
static constexpr uint32_t kPchResetHandshakeEnable = 0x00000010;
static constexpr uint32_t kPowerWellControl1 = 0x00045400;
static constexpr uint32_t kPowerWellControl2 = 0x00045404;
static constexpr uint32_t kPowerWellControl3 = 0x00045408;
static constexpr uint32_t kPowerWellControl4 = 0x0004540C;
static constexpr uint32_t kDisplayFuseStatus = 0x00042000;
static constexpr uint32_t kDisplayBufferControl = 0x00045008;
static constexpr uint32_t kDDIBufferControlE = 0x00064400;
static constexpr uint32_t kDDIBufferIdle = 0x00000080;
static constexpr uint32_t kDPTransportEnable = 0x80000000;
static constexpr uint32_t kDPTransportPatternMask = 0x00000700;
static constexpr uint32_t kDPTransportPattern1 = 0x00000000;

// Coffee Lake eDP-transcoder Panel Self Refresh registers.  Tahoe enables
// PSR globally on this machine (ASRDisable=0, MaximumSelfRefreshLevel=3), but
// IORegistry shows no BanksiaTcon/CamelliaTcon2 instance for the non-Apple
// Samsung panel.  AppleCamellia's native SRDExit writes 0x00101001 to
// EDP_PSR_CTL and waits for EDP_PSR_STATUS[31:29] to become idle.  Upstream
// i915 additionally clears PSR2 first and disables sink PSR at DPCD 0x170.
// V44+ applies that bounded, documented exit only after a committed S3 wake,
// after the sink is back in D0, and before link training.
static constexpr uint32_t kEDPPSRControl = 0x0006F800;
static constexpr uint32_t kEDPPSRStatus = 0x0006F840;
static constexpr uint32_t kEDPPSREvent = 0x0006F848;
static constexpr uint32_t kEDPPSRDebug = 0x0006F860;
static constexpr uint32_t kEDPPSR2Control = 0x0006F900;
static constexpr uint32_t kEDPPSR2Status = 0x0006F940;
static constexpr uint32_t kPipeAStatus = 0x00070030;
static constexpr uint32_t kPSREnable = 0x80000000;
static constexpr uint32_t kPSRStatusStateMask = 0xE0000000;
static constexpr uint32_t kPSR2StatusStateMask = 0xF0000000;
static constexpr uint32_t kAppleCamelliaPSRDisabledControl = 0x00101001;
static constexpr uint32_t kDPCDPSRSupport = 0x00000070;
static constexpr uint32_t kDPCDPSREnableConfig = 0x00000170;
// Windows 31.0.101.2141 calls EMBEDDED_DPSINK::SetEdpConfiguration with
// argument 1 immediately before TrainLink on this exact machine.  The method
// writes byte 0x01 to eDP_CONFIGURATION_SET (DPCD 0x10A).  V53 proved that an
// earlier write cannot test this state: Tahoe's LinkTraining immediately
// generates its own one-byte value from ASREnabled=0 and writes 0x00 over it.
// V54 therefore intercepts only that exact native LinkTraining WriteAUX call
// and substitutes the Windows byte on the already bounded S3/eDP path.
static constexpr uint32_t kDPCDEDPConfigurationSet = 0x0000010A;
static constexpr uint8_t kWindowsEDPConfigurationSet = 0x01;
// This Samsung eDP 1.4 sink exposes a supported-link-rate table at DPCD
// 0x010.  Both the successful Linux i915 path and the machine's Windows
// 31.0.101.2141 driver select HBR2 through LINK_RATE_SET (0x115) and write
// LANE_COUNT_SET (0x101) separately; Tahoe instead uses the legacy two-byte
// LINK_BW_SET write at 0x100.  V61 substitutes only Tahoe 25.2's one initial
// LinkTraining call, only after the live sink table exactly proves selector 2
// is 5.4 Gbit/s, and falls back to Apple's original write on every mismatch.
static constexpr uint32_t kDPCDSupportedLinkRates = 0x00000010;
static constexpr uint32_t kDPCDLinkBandwidthSet = 0x00000100;
static constexpr uint32_t kDPCDLaneCountSet = 0x00000101;
static constexpr uint32_t kDPCDLinkRateSet = 0x00000115;
static constexpr uint8_t kAppleHBR2LegacyBandwidth = 0x14;
static constexpr uint8_t kAppleFourLaneEnhanced = 0x84;
static constexpr uint8_t kWindowsLinuxHBR2RateSelector = 0x02;
static constexpr uint8_t kLinkRateSelectorMask = 0x07;
static constexpr uint16_t kSupportedRateRBR20KHz = 0x1FA4;
static constexpr uint16_t kSupportedRateHBR20KHz = 0x34BC;
static constexpr uint16_t kSupportedRateHBR220KHz = 0x6978;
static constexpr uint32_t kDPCDSetPower = 0x00000600;
static constexpr uint8_t kDPCDSetPowerD0 = 0x01;
static constexpr uint32_t kDPCDTrainingLane0Set = 0x00000103;
static constexpr uint8_t kDPTrainingSwing2Pre0 = 0x06;
static constexpr uint8_t kDPTrainingSwing3Pre0 = 0x07;
static constexpr uint32_t kDDITrainingSelectorMask = 0x0F000000;
// Tahoe's gLTDriveTable maps both V2/P0 and V3/P0 to source selector 7, and
// both V2/P1 and V3/P1 to selector 8.  The selector therefore cannot identify
// which of the two native DPCD bytes (0x06 or 0x07) Apple will write next.
static constexpr uint32_t kDDITrainingSelectorMaxPre0 = 0x07000000;
static constexpr uint32_t kPSRExitPollDelayMs = 10;
static constexpr uint32_t kPSRExitPollCount = 200;

// Gen9/CML display power-well fields from HSW_PWR_WELL_CTL2.  Port A and
// Port E share one analog DDI I/O well on this generation.  The request bits
// are writable by the graphics driver; the adjacent state bits are read-only.
// Linux's Gen9 resume path first restores PG1 and MISC_IO, keeps DBUF powered,
// then acquires DDI_IO_A_E before link training.  V41 proved all four lanes
// still report zero clock recovery even though the A/E well reads enabled.
// V42 proved that clearing the driver request alone cannot turn this well off:
// the Razer firmware's BIOS request remains asserted in CTL1.  Intel i915's
// hsw_power_well_sync_hw() explicitly transfers such a BIOS request to the
// driver and clears the BIOS request before normal power-well management.
// V43-V49 performed that ownership handoff and then forced one bounded
// DDI_IO_A_E off/on cycle.  The cycle completed, but the sink still reported
// zero clock recovery on every lane.  Intel's hsw_power_well_sync_hw() stops
// after the ownership transfer: assert the driver request first, clear the
// BIOS request, and keep the IO well continuously powered.  V50 follows that
// reference sequence so firmware-retained analog PHY calibration is not lost.
static constexpr uint32_t kPowerWellPW1Request = 0x20000000;
static constexpr uint32_t kPowerWellPW1State = 0x10000000;
static constexpr uint32_t kPowerWellMiscIORequest = 0x00000002;
static constexpr uint32_t kPowerWellMiscIOState = 0x00000001;
static constexpr uint32_t kPowerWellDDIAERequest = 0x00000008;
static constexpr uint32_t kPowerWellDDIAEState = 0x00000004;
static constexpr uint32_t kDBUFPowerRequest = 0x80000000;
static constexpr uint32_t kDBUFPowerState = 0x40000000;
static constexpr uint32_t kPowerWellPollDelayUs = 10;
static constexpr uint32_t kPowerWellPollCount = 500;
static constexpr uint32_t kTargetIntelVendorID = 0x8086;
static constexpr uint32_t kTargetCMLGT2DeviceID = 0x9BC4;
static constexpr uint32_t kTargetCMP2PCHDeviceID = 0x068D;

// Windows 31.0.101.2141's successful S3 D0 path reaches its RAWCLK restore
// helper at PE 0x1401EB700 before modeset and link training.  ETW sampled the
// helper at 0x1401EB762, after it read both PCH_RAWCLK_FREQ and SFUSE_STRAP and
// immediately before it recomputed and wrote the clock.  Tahoe programs the
// same two registers only from initDisplayEngine(), which normally bails out
// once the engine is initialized.  Preserve this machine's known-working cold
// value and re-latch that exact value during the first display power-well D0
// callback after S3.  The Apple and Windows formulas are published only as
// diagnostics; neither guessed formula replaces the proven cold value.
static constexpr uint32_t kPchRawClockFrequency = 0x000C6204;
static constexpr uint32_t kSouthFuseStrap = 0x000C2014;
// Linux's CNP/CMP display suspend path applies Tweaked Wa_14010685332 by
// setting SOUTH_CHICKEN1.SBCLK_RUN_REFCLK_DIS after display power-down and
// clearing it in resume-early before display power domains are restored.
// This machine's 0x068d CMP2 LPC device maps to Linux PCH_CNP, and the same
// 0xC2000 register is present in the successful Windows Intel driver.  Mirror
// only that documented bit, preserve every unrelated field, and require a
// known-working cold value before either write.
static constexpr uint32_t kSouthChicken1 = 0x000C2000;
static constexpr uint32_t kSBCLKRunRefclkDisable = 1U << 7;
// CNP Display WA #1179 is independent of the suspend/resume bit above.  Linux
// writes CHASSIS_CLK_REQ_DURATION=0xF immediately before HPD detection setup;
// Windows 31.0.101.2141 likewise checks SOUTH_CHICKEN1[11:8] and ORs 0xF00
// before its HPD/interrupt setup.  This machine is cold-working at 0xF00 but
// reaches the first real-S3 panel-on at 0x900.  V57 restores only this complete
// field, once, immediately before the existing Windows-style HPD controls.
static constexpr uint32_t kChassisClockRequestDurationMask = 0x00000F00;
static constexpr uint32_t kChassisClockRequestDurationColdExpected =
	0x00000F00;
static constexpr uint32_t kChassisClockRequestDurationWakeExpected =
	0x00000900;
static constexpr uint32_t kSouthChickenSleepSetMaxAttempts = 1;
static constexpr uint32_t kSouthChickenWakeClearMaxAttempts = 4;
static constexpr uint32_t kSouthFuseRawFrequency24MHz = 0x00000100;
static constexpr uint32_t kAppleRawClockPreserveMask = 0xC000FFFF;
static constexpr uint32_t kAppleRawClock24MHz = 0x00170000;
static constexpr uint32_t kAppleRawClock19Point2MHz = 0x10120000;
static constexpr uint32_t kWindowsRawClock24MHzPreserveMask = 0xC018FFFF;
static constexpr uint32_t kWindowsRawClock24MHz = 0x00180000;
static constexpr uint32_t kWindowsRawClock19Point2MHzPreserveMask =
	0xD012FFFF;
static constexpr uint32_t kWindowsRawClock19Point2MHz = 0x10120000;
static constexpr uint32_t kRawClockRelatchDelayUs = 10;

// Read only the standard DP/eDP blocks that are known to exist on this panel.
// A display-only wake succeeds because receiver power is retained, while a
// real S3 wake fails after the receiver loses power.  Capturing the working
// post-training state and the cold-reset pre-training state lets us identify
// volatile sink configuration without writing undocumented Samsung DPCD
// locations.  Each record is self-describing so a black-screen watchdog can
// save and compare it directly from IORegistry.
static constexpr uint32_t kDPCDSnapshotAddresses[] {
	0x0000, // Receiver capabilities.
	0x0010, // eDP supported link-rate table.
	0x0070, // PSR capability and setup-time block.
	0x0100, // Link configuration and training controls.
	0x0110, // eDP 1.4 LINK_RATE_SET and adjacent link controls.
	0x0170, // PSR enable/configuration block.
	0x0200, // Sink count, lane status, and adjust requests.
	0x0600, // SET_POWER and power-state neighborhood.
	0x0700, // eDP revision and general capabilities.
	0x0710, // eDP display/backlight capability block.
	0x0720, // eDP backlight and regional capability block.
	0x0730, // eDP extended control block.
	0x2200, // Extended receiver capabilities used by modern eDP sinks.
	0x2210
};
static constexpr uint32_t kDPCDSnapshotBlockSize = 16;
static constexpr uint32_t kDPCDSnapshotBlockCount =
	arrsize(kDPCDSnapshotAddresses);

struct DPCDSnapshotRecord {
	uint32_t address;
	uint32_t result;
	uint8_t data[kDPCDSnapshotBlockSize];
};

// The display work loop serialises panel power and LightUpEDP calls, so this
// flag deliberately remains a simple scalar instead of adding a lock inside
// the graphics driver's power path.
static volatile uint32_t panelNeedsD0Precondition = 0;
// Apple powers the panel once before LightUpEDP.  On this OLED that first S3
// power-up can leave PP_STATUS=on while the eDP receiver itself never exits
// reset (AUX works, HPD stays low, and every clock-recovery lane is zero).
// Permit one deliberate receiver power cycle per committed system sleep, but
// never repeat it for ordinary display blanking or later modeset retries.
static volatile uint32_t wakePanelResetAttempted = 0;
static volatile uint32_t forceRazerPanelPowerConfig = 0;
// A real system sleep is announced before IOGraphics asks the panel to enter
// D3.  Keep display-only blanking non-destructive, but let the Samsung OLED
// complete its normal eDP power-down sequence before the firmware removes the
// S3 rails.  Otherwise the panel controller is cut off mid-transition.
static volatile uint32_t systemSleepPending = 0;
// The ordinary sleep notifier used by V57 never receives shutdown/restart
// messages.  The resulting state=0 call could therefore be mistaken for a
// display-only blank and suppressed.  V60 combines the early ordinary sleep
// notifier with a separate priority termination notifier and this explicit
// gate, so shutdown, restart and paging-off always take Apple's native
// physical panel-off path.
static volatile uint32_t systemTerminationPending = 0;
static volatile uint32_t savedDisplayIOBalanceLeg = 0;
static volatile uint32_t savedDisplayIOBalanceLegValid = 0;
static uint32_t savedDDIBufferTranslation[kDDIBufferTranslationDwordCount] {};
static volatile uint32_t savedDDIBufferTranslationValid = 0;
// 0 = unrelated ConfigureBufferTranslation call, 1 = first known-good eDP
// LightUpEDP capture, 2 = S3 wake restore after Apple's own table write.
static volatile uint32_t bufferTranslationMode = 0;
static volatile uint32_t wakeTranslationRestoreAttempts = 0;
static volatile uint32_t trainingDiagnosticsArmed = 0;
static volatile uint32_t trainingCheckCount = 0;
static volatile uint32_t phase1ClockRecoveryDelayCount = 0;
static volatile uint32_t nativeEDPConfigurationInterceptCount = 0;
static volatile uint32_t nativeLinkRateSelectInterceptCount = 0;
static volatile uint32_t ddiBufferActivationSettleCount = 0;
static volatile uint32_t initialCarrierActivationCount = 0;
static volatile uint32_t initialCarrierRepairCount = 0;
static volatile uint32_t wakeHotPlugRestoreCount = 0;
static volatile uint32_t wakeHPDReadyAfterNativePanelOn = 0;
// V55 incorrectly treated the cold snapshot's final Phase-2 0x0E lane bytes
// as a Phase-1 clock-recovery target.  A working native modeset proves the
// required order is 0x00 -> 0x01 -> 0x06 while TPS1 is active; only after
// clock recovery reaches 0x1111 does Tahoe enter TPS3 and naturally select
// 0x0E for channel equalisation.  V56 never substitutes either side.  These
// counters audit the exact native Phase-1 source and sink values unchanged.
static volatile uint32_t phase1NativeSourceWriteCount = 0;
static volatile uint32_t phase1NativeSinkWriteCount = 0;
static volatile uint32_t phase1NativeSink06Count = 0;
static volatile uint32_t phase1NativeSink07Count = 0;
static volatile uint32_t phase1NativeSinkOtherCount = 0;
static volatile uint32_t targetCMLIGPUValidated = 0;
static volatile uint32_t v61PatchFullyArmed = 0;
static volatile uint32_t linuxKBLIBoostApplyCount = 0;
static volatile uint32_t linuxKBLIBoostVerifiedCount = 0;
static volatile uint32_t linuxKBLIBoostRefusedCount = 0;
static volatile uint32_t phase2NativeSourceWriteCount = 0;
static volatile uint32_t cmlPhyReinitArmed = 0;
static volatile uint32_t cmlPhyReinitAttempted = 0;
static volatile uint32_t cmlPhyReinitCount = 0;
static volatile uint32_t savedPchRawClockFrequency = 0;
static volatile uint32_t savedPchRawClockFrequencyValid = 0;
static volatile uint32_t savedSouthFuseStrap = 0;
static volatile uint32_t wakeRawClockRestoreAttempted = 0;
static volatile uint32_t savedSouthChicken1 = 0;
static volatile uint32_t savedSouthChicken1Valid = 0;
static volatile uint32_t southChickenPanelOffCompleted = 0;
static volatile uint32_t southChickenSuspendSetArmed = 0;
static volatile uint32_t southChickenSleepSetAttempted = 0;
static volatile uint32_t southChickenSleepSetWriteIssued = 0;
static volatile uint32_t southChickenSleepSetVerified = 0;
static volatile uint32_t southChickenWakeClearArmed = 0;
static volatile uint32_t southChickenWakeClearAttempted = 0;
static volatile uint32_t southChickenWakeClearVerified = 0;
static volatile uint32_t southChickenResumeBoundaryObserved = 0;
static volatile uint32_t southChickenDurationRestoreArmed = 0;
static volatile uint32_t southChickenDurationRestoreAttempted = 0;
static volatile uint32_t southChickenDurationRestoreWriteIssued = 0;
static volatile uint32_t southChickenDurationRestoreVerified = 0;
static volatile uint32_t southChickenDurationFirstTrainingSampled = 0;
static volatile uint32_t igpuDeviceWillPowerOffCount = 0;
static volatile uint32_t igpuDeviceWillNotPowerOffCount = 0;
static volatile uint32_t igpuDeviceHasPoweredOffCount = 0;
static volatile uint32_t igpuDeviceWillPowerOnCount = 0;
static volatile uint32_t igpuDeviceHasPoweredOnCount = 0;
static volatile uint32_t framebufferSleepToWakeCount = 0;
static volatile uint32_t savedSetupDDIBufferControl = 0;
static volatile uint32_t savedSetupDDIBufferControlValid = 0;
static DPCDSnapshotRecord savedColdDPCD[kDPCDSnapshotBlockCount] {};
static volatile uint32_t savedColdDPCDValid = 0;
static IONotifier *sleepWakeNotifier = nullptr;
static IONotifier *priorityPowerNotifier = nullptr;
static IONotifier *igpuPowerNotifier = nullptr;
static IOPCIDevice *igpuPowerService = nullptr;
static IOMemoryMap *igpuBAR0Map = nullptr;
static volatile uint8_t *igpuBAR0Address = nullptr;
static volatile uint64_t igpuBAR0Length = 0;
static volatile uint32_t igpuBAR0MappingValid = 0;
static IOLock *southChickenLock = nullptr;
static IOMemoryDescriptor *pchGpioDescriptor = nullptr;
static IOMemoryMap *pchGpioMap = nullptr;
static volatile uint8_t *pchGpioAddress = nullptr;
static volatile uint32_t pchGpioMappingValid = 0;
static volatile uint32_t coldLVddEnDw0 = 0;
static volatile uint32_t coldLVddEnDw0Valid = 0;

static bool isInternalFramebuffer(void *framebuffer);
static bool isInternalEDPPath(void *framebuffer, void *displayPath);
static bool restoreSouthChickenReferenceClockBeforeResume(const char *stage);
static bool restoreSouthChickenHotPlugDurationBeforePanelOn(void *controller,
	const char *stage);

static const uint8_t findPanelSettleDelay[] {
	0x41, 0xBF, 0x32, 0x00, 0x00, 0x00,
	0x41, 0xBD, 0xFB, 0x8D, 0xF3, 0xFF,
	0x41, 0xBC, 0x00, 0x00, 0x00, 0xB0,
	0xBF, 0x19, 0x00, 0x00, 0x00
};

static const uint8_t replacePanelSettleDelay[] {
	0x41, 0xBF, 0x32, 0x00, 0x00, 0x00,
	0x41, 0xBD, 0xFB, 0x8D, 0xF3, 0xFF,
	0x41, 0xBC, 0x00, 0x00, 0x00, 0xB0,
	0xBF, 0xF4, 0x01, 0x00, 0x00
};

// LinkTraining Phase 1 stops after the sink repeats the same voltage request
// five times.  The Razer OLED does not raise HPD until roughly 34 ms after
// that limit is reached, so allow five additional AUX/status cycles.
static const uint8_t findPhase1StableRetryExit[] {
	0x41, 0x80, 0xFD, 0x04, 0x77, 0x6B,
	0x48, 0x8B, 0x8D, 0x78, 0xFF, 0xFF, 0xFF
};

static const uint8_t replacePhase1StableRetryExit[] {
	0x41, 0x80, 0xFD, 0x09, 0x77, 0x6B,
	0x48, 0x8B, 0x8D, 0x78, 0xFF, 0xFF, 0xFF
};

static const uint8_t findPhase1StableRetryFailure[] {
	0x89, 0x55, 0xA8,
	0x41, 0x80, 0xFD, 0x05,
	0x0F, 0x84, 0xE5, 0x04, 0x00, 0x00,
	0x45, 0x84, 0xF6
};

static const uint8_t replacePhase1StableRetryFailure[] {
	0x89, 0x55, 0xA8,
	0x41, 0x80, 0xFD, 0x0A,
	0x0F, 0x84, 0xE5, 0x04, 0x00, 0x00,
	0x45, 0x84, 0xF6
};

// Tahoe's regular LinkTraining writes DP_TP_CTL_A for Pattern 1, then performs
// this first DDI_BUF_CTL_A enable, and immediately arms sink DPCD 0x102.  V47
// intercepted only the later Phase-1 voltage-adjustment write, after the sink
// had already been told to sample Pattern 1.  Retarget this distinct call so a
// real S3 wake verifies/repairs the source training-pattern state and gives the
// reconstructed DDIA PHY a bounded activation interval before the sink is
// armed.  The byte sequence is unique in Tahoe 25.2 LinkTraining.
static const uint8_t findInitialDDIBufferEnableCallSite[] {
	0x4C, 0x8B, 0x6D, 0xA8,
	0x41, 0x83, 0xE5, 0x07,
	0x42, 0x8D, 0x14, 0x6D, 0x10, 0x00, 0x00, 0x80,
	0x41, 0x8B, 0x74, 0x24, 0x30,
	0x4C, 0x89, 0xF7,
	0x89, 0x95, 0x48, 0xFF, 0xFF, 0xFF,
	0xE8, 0x62, 0x21, 0x02, 0x00,
	0x4C, 0x8D, 0x45, 0xD7,
	0x41, 0xC6, 0x00, 0x21
};
static constexpr size_t kInitialDDIBufferEnableCallOffset = 30;

// Unique Tahoe 25.2 LinkTraining Phase-1 sequence surrounding the Port A DDI
// source-strength write at LinkTraining+0x6E3.  V45 accidentally targeted the
// structurally similar Phase-2 write at +0xB58, which cannot run while clock
// recovery is still failing.  Only the five-byte Phase-1 call is redirected;
// no global WriteRegister32 hook is installed.
static const uint8_t findDDIBufferEnableCallSite[] {
	0x4C, 0x8B, 0x7D, 0xA0,
	0x41, 0x8B, 0x77, 0x30,
	0x4C, 0x8B, 0x65, 0xC8,
	0x4C, 0x89, 0xE7,
	0x89, 0x95, 0x68, 0xFF, 0xFF, 0xFF,
	0xE8, 0x2A, 0x20, 0x02, 0x00,
	0xB8, 0x33, 0x33, 0x00, 0x00
};
static constexpr size_t kDDIBufferEnableCallOffset = 21;

// Unique Tahoe 25.2 LinkTraining Phase-2 source-strength write.  Clock
// recovery never reached this call in V57, but Linux applies the same
// selector-specific I_boost update in both phases.  Retargeting the distinct
// call at LinkTraining+0xB58 lets selectors 3/6/8 restore boost 1 for channel
// equalisation while all other selectors retain Linux's explicit disable.
static const uint8_t findPhase2DDIBufferEnableCallSite[] {
	0x44, 0x89, 0xE2,
	0x83, 0xE2, 0x0F,
	0xC1, 0xE2, 0x18,
	0x0B, 0x95, 0x48, 0xFF, 0xFF, 0xFF,
	0x48, 0x8B, 0x45, 0xA0,
	0x8B, 0x70, 0x30,
	0x45, 0x89, 0xF7,
	0x4C, 0x8B, 0x75, 0xC8,
	0x4C, 0x89, 0xF7,
	0x89, 0x95, 0x38, 0xFF, 0xFF, 0xFF,
	0xE8, 0xB5, 0x1B, 0x02, 0x00,
	0xB8, 0x33, 0x33, 0x00, 0x00
};
static constexpr size_t kPhase2DDIBufferEnableCallOffset = 38;

// Unique Tahoe 25.2 phase-1 lane-set write in the adjustment loop.  It follows
// the DDI_BUF_CTL source-strength write above and writes four lane bytes to
// standard DPCD TRAINING_LANE0_SET (0x103).  Retargeting this one call avoids
// any global WriteAUX interception.
static const uint8_t findDPCDLaneSetWriteCallSite[] {
	0x4C, 0x89, 0xE7,
	0x4C, 0x8B, 0x75, 0x80,
	0x4C, 0x89, 0xF6,
	0xBA, 0x03, 0x01, 0x00, 0x00,
	0xB9, 0x04, 0x00, 0x00, 0x00,
	0x4C, 0x8D, 0x45, 0xC0,
	0x4D, 0x89, 0xF9,
	0xE8, 0x67, 0xC7, 0xFF, 0xFF,
	0x85, 0xC0
};
static constexpr size_t kDPCDLaneSetWriteCallOffset = 27;

// Unique prefix of Tahoe 25.2 LinkTraining's Phase-1 wait.  The first call is
// IODelay for sub-millisecond intervals; the second call, at +0x20, is IOSleep
// for this panel's 8 ms interval.  Imported-call displacements are relocated at
// load time, so the search ends before the first E8 and the surrounding opcode
// shape is verified separately before the second call is changed.
static const uint8_t findPhase1ClockRecoveryDelayCallSite[] {
	0x81, 0xBD, 0x38, 0xFF, 0xFF, 0xFF, 0xE7, 0x03, 0x00, 0x00,
	0x77, 0x0E,
	0x48, 0x8B, 0xBD, 0x50, 0xFF, 0xFF, 0xFF
};
static constexpr size_t kPhase1ClockRecoveryDelayCallOffset = 0x20;

// Tahoe 25.2 LinkTraining+0x3DA builds the native two-byte link configuration
// on the stack and calls WriteAUX(0x100, 2) at +0x413.  Match the complete
// argument construction so no later training-pattern/lane write can be
// mistaken for this call.  The imported-call displacement is relocated at
// load time and is therefore verified separately before being retargeted.
static const uint8_t findNativeLinkConfigurationWriteCallSite[] {
	0x4C, 0x8D, 0x45, 0xB2,
	0x8B, 0x45, 0x8C,
	0x41, 0x88, 0x00,
	0x44, 0x8B, 0x6D, 0x90,
	0x45, 0x84, 0xED,
	0x0F, 0x95, 0xC0,
	0xC0, 0xE0, 0x07,
	0x0A, 0x45, 0xBC,
	0x41, 0x88, 0x40, 0x01,
	0x48, 0x8B, 0x7D, 0xC8,
	0x4C, 0x89, 0xE3,
	0x4C, 0x89, 0xE6,
	0xBA, 0x00, 0x01, 0x00, 0x00,
	0xB9, 0x02, 0x00, 0x00, 0x00,
	0x4C, 0x8B, 0x65, 0xA0,
	0x4D, 0x89, 0xE1
};
static constexpr size_t kNativeLinkConfigurationWriteCallOffset = 0x39;

// Tahoe 25.2 LinkTraining+0x481 builds its DPCD 0x10A byte directly from the
// local ASREnabled boolean, then calls WriteAUX at +0x4A4.  V53's live AUX log
// captured this call writing 0x00 immediately after the earlier Windows-value
// write.  Match the full argument setup (but not the relocated call target),
// then verify the call and its success branch separately.  This prefix occurs
// exactly once in the supported 1,104,640-byte framebuffer executable.
static const uint8_t findNativeEDPConfigurationWriteCallSite[] {
	0x80, 0xBD, 0x6C, 0xFF, 0xFF, 0xFF, 0x00,
	0x4C, 0x8D, 0x45, 0xB2,
	0x41, 0x0F, 0x95, 0x00,
	0x48, 0x8B, 0x7D, 0xC8,
	0x48, 0x89, 0xDE,
	0xBA, 0x0A, 0x01, 0x00, 0x00,
	0xB9, 0x01, 0x00, 0x00, 0x00,
	0x4D, 0x89, 0xE1
};
static constexpr size_t kNativeEDPConfigurationWriteCallOffset = 35;
static constexpr size_t kRelativeCallSize = 5;

static_assert(sizeof(findPanelSettleDelay) == sizeof(replacePanelSettleDelay),
	"Razer OLED find and replace sequences must have equal size");
static_assert(sizeof(findPhase1StableRetryExit) == sizeof(replacePhase1StableRetryExit),
	"Razer OLED Phase 1 exit sequences must have equal size");
static_assert(sizeof(findPhase1StableRetryFailure) == sizeof(replacePhase1StableRetryFailure),
	"Razer OLED Phase 1 failure sequences must have equal size");
static const char *currentPatchStatus = "not-started";

extern "C" const char *RazerOLEDWakeFix_getPatchStatus() {
	return currentPatchStatus;
}

static void publishPatchStatus(const char *status) {
	currentPatchStatus = status;
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("PatchStatus", status);
}

static uint32_t readPchGpio32(uint32_t offset) {
	if (pchGpioMappingValid == 0 || pchGpioAddress == nullptr ||
		offset > kPchGpioCommunityLength - sizeof(uint32_t))
		return 0xFFFFFFFF;

	OSSynchronizeIO();
	const auto value = *reinterpret_cast<volatile uint32_t *>(
		pchGpioAddress + offset);
	OSSynchronizeIO();
	return value;
}

static uint32_t readIGPUDisplayMMIO32(uint32_t offset) {
	if (igpuBAR0MappingValid == 0 || igpuBAR0Address == nullptr ||
		igpuBAR0Length < sizeof(uint32_t) ||
		static_cast<uint64_t>(offset) >
			igpuBAR0Length - sizeof(uint32_t))
		return 0xFFFFFFFFU;

	OSSynchronizeIO();
	const auto value = *reinterpret_cast<volatile uint32_t *>(
		igpuBAR0Address + offset);
	OSSynchronizeIO();
	return value;
}

static void writeIGPUDisplayMMIO32(uint32_t offset, uint32_t value) {
	if (igpuBAR0MappingValid == 0 || igpuBAR0Address == nullptr ||
		igpuBAR0Length < sizeof(uint32_t) ||
		static_cast<uint64_t>(offset) >
			igpuBAR0Length - sizeof(uint32_t))
		return;

	OSSynchronizeIO();
	*reinterpret_cast<volatile uint32_t *>(igpuBAR0Address + offset) = value;
	OSSynchronizeIO();
}

static void writePchGpio32(uint32_t offset, uint32_t value) {
	if (pchGpioMappingValid == 0 || pchGpioAddress == nullptr ||
		offset > kPchGpioCommunityLength - sizeof(uint32_t))
		return;

	*reinterpret_cast<volatile uint32_t *>(pchGpioAddress + offset) = value;
	OSSynchronizeIO();
}

static void publishPchPanelPadSnapshot(const char *propertyName,
	bool captureCold = false) {
	if (pchGpioMappingValid == 0)
		return;

	uint32_t values[] {
		readPchGpio32(kPchGpioRevision),
		readPchGpio32(kPchGpioPadBar),
		readPchGpio32(kGppFPadCfgLock),
		readPchGpio32(kGppFPadCfgLockTx),
		readPchGpio32(kGppFLVddEnDw0),
		readPchGpio32(kGppFLVddEnDw1),
		readPchGpio32(kGppFLBkltEnDw0),
		readPchGpio32(kGppFLBkltEnDw1),
		readPchGpio32(kGppFLBkltCtlDw0),
		readPchGpio32(kGppFLBkltCtlDw1)
	};
	if (captureCold) {
		coldLVddEnDw0 = values[4];
		OSSynchronizeIO();
		coldLVddEnDw0Valid = 1;
	}
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty(propertyName, values,
			sizeof(values));

	SYSLOG("gpio", "%s: REVID=%08X PADBAR=%08X LOCK=%08X/%08X L_VDDEN=%08X/%08X L_BKLTEN=%08X/%08X L_BKLTCTL=%08X/%08X",
		propertyName, values[0], values[1], values[2], values[3],
		values[4], values[5], values[6], values[7], values[8], values[9]);
}

static bool initPchPanelGpioMapping() {
	if (pchGpioMappingValid != 0)
		return true;

	pchGpioDescriptor = IOMemoryDescriptor::withPhysicalAddress(
		kPchGpioCommunityPhysical, kPchGpioCommunityLength,
		kIODirectionInOut);
	if (pchGpioDescriptor == nullptr) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("PchPanelGpioStatus",
				"physical-descriptor-failed");
		return false;
	}

	pchGpioMap = pchGpioDescriptor->map(kIOMapInhibitCache);
	if (pchGpioMap == nullptr || pchGpioMap->getAddress() == 0) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("PchPanelGpioStatus",
				"uncached-map-failed");
		return false;
	}
	pchGpioAddress = reinterpret_cast<volatile uint8_t *>(
		pchGpioMap->getAddress());
	pchGpioMappingValid = 1;
	const auto padBar = readPchGpio32(kPchGpioPadBar);
	if (padBar != kExpectedPchCommunityPadBar) {
		pchGpioMappingValid = 0;
		if (RazerOLEDWakeFix_selfInstance != nullptr) {
			RazerOLEDWakeFix_selfInstance->setProperty("PchPanelGpioStatus",
				"refused-padbar-mismatch");
			RazerOLEDWakeFix_selfInstance->setProperty("PchPanelGpioObservedPadBar",
				static_cast<uint64_t>(padBar), 32);
		}
		SYSLOG("gpio", "refused PCH GPIO mapping at 0x%llX: PADBAR=%08X expected=%08X",
			static_cast<unsigned long long>(kPchGpioCommunityPhysical),
			padBar, kExpectedPchCommunityPadBar);
		return false;
	}

	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty("PchPanelGpioStatus",
			"mapped-validated-cannonlake-h-gpp-f");
		RazerOLEDWakeFix_selfInstance->setProperty("PchPanelGpioPhysical",
			static_cast<uint64_t>(kPchGpioCommunityPhysical), 64);
		RazerOLEDWakeFix_selfInstance->setProperty("PchPanelGpioPadBar",
			static_cast<uint64_t>(padBar), 32);
	}
	publishPchPanelPadSnapshot("ColdPchPanelPads", true);
	return true;
}

static bool restoreLVddEnNativeModeIfNeeded() {
	if (pchGpioMappingValid == 0 || coldLVddEnDw0Valid == 0) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("WakeLVddEnModeRestoreStatus",
				"skipped-no-validated-cold-snapshot");
		return false;
	}

	const auto before = readPchGpio32(kGppFLVddEnDw0);
	const auto lock = readPchGpio32(kGppFPadCfgLock);
	const auto requested = (before & ~kPadCfg0PModeMask) |
		(coldLVddEnDw0 & kPadCfg0PModeMask);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty("WakeLVddEnModeBefore",
			static_cast<uint64_t>(before), 32);
		RazerOLEDWakeFix_selfInstance->setProperty("WakeLVddEnModeCold",
			static_cast<uint64_t>(coldLVddEnDw0), 32);
		RazerOLEDWakeFix_selfInstance->setProperty("WakeLVddEnModeRequested",
			static_cast<uint64_t>(requested), 32);
	}

	if (requested == before) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("WakeLVddEnModeRestoreStatus",
				"not-needed-native-mode-matched-cold-boot");
		return true;
	}
	if ((lock & kGppFLVddEnLockBit) != 0) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("WakeLVddEnModeRestoreStatus",
				"refused-pad-configuration-locked");
		SYSLOG("gpio", "L_VDDEN PMODE differs after S3 but GPP_F PADCFGLOCK=%08X has bit 19 set; no write attempted",
			lock);
		return false;
	}

	writePchGpio32(kGppFLVddEnDw0, requested);
	IODelay(10);
	const auto after = readPchGpio32(kGppFLVddEnDw0);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty("WakeLVddEnModeAfter",
			static_cast<uint64_t>(after), 32);
		RazerOLEDWakeFix_selfInstance->setProperty("WakeLVddEnModeRestoreStatus",
			after == requested ? "restored-cold-native-mode-verified" :
			"restore-write-did-not-stick");
	}
	SYSLOG("gpio", "L_VDDEN PMODE exact cold restore: before=%08X cold=%08X requested=%08X after=%08X verified=%u",
		before, coldLVddEnDw0, requested, after, after == requested);
	return after == requested;
}

// The priority interest is deliberately limited to termination.  Tahoe sends
// its priority kIOMessageSystemWillSleep only after the display has already
// entered Doze/D3, which is too late to distinguish the panel state=0 call
// from an ordinary display blank.  It does, however, deliver shutdown/restart
// early enough to force the native physical panel-off path.
static IOReturn priorityTerminationInterest(void *, void *, UInt32 messageType,
	IOService *, void *, vm_size_t) {
	switch (messageType) {
		case kIOMessageSystemWillPowerOff:
		case kIOMessageSystemWillRestart:
		case kIOMessageSystemPagingOff:
			{
				// This is the priority shutdown path.  Publish the termination gate
				// first, then disarm every custom transition with only scalar stores.
				// Do not take a display-path lock, touch IORegistry or log here: any of
				// those can delay the native shutdown/restart panel-off sequence.
				systemTerminationPending = 1;
				OSSynchronizeIO();
				systemSleepPending = 0;
				southChickenSuspendSetArmed = 0;
				southChickenDurationRestoreArmed = 0;
				southChickenPanelOffCompleted = 0;
				cmlPhyReinitArmed = 0;
				panelNeedsD0Precondition = 0;
				forceRazerPanelPowerConfig = 0;
				OSSynchronizeIO();
				break;
			}
		default:
			break;
	}
	return kIOReturnSuccess;
}

// The ordinary sleep/wake interest is intentionally separate from the
// priority termination interest.  It receives the committed WillSleep before
// IOGraphics powers FB0 down, so the existing state=0 wrapper can take the
// working BMU snapshot, perform a native panel-off, and arm the S3-only Linux
// source-strength path.  This is the timing proven by the V57 captures.
static IOReturn systemPowerInterest(void *, void *, UInt32 messageType,
	IOService *, void *, vm_size_t) {
	switch (messageType) {
		case kIOMessageCanSystemSleep:
			// Do not arm for an abortable query.  This synchronous general-interest
			// callback acknowledges the query by returning kIOReturnSuccess; its
			// message argument is an IOPowerStateChangeNotification, not an ack cookie.
			break;
		case kIOMessageSystemWillSleep:
			{
				// A priority termination message always wins a race with sleep.
				// Never clear its gate or arm any display-path modification.
				if (systemTerminationPending != 0)
					break;
				// Never erase evidence that a previous generation may still have
				// SOUTH_CHICKEN1.bit7 set.  At this point the system is fully awake,
				// so make one bounded recovery attempt before arming a new sequence.
				const bool previousClearWasArmed =
					southChickenWakeClearArmed != 0;
				if (previousClearWasArmed)
					(void)restoreSouthChickenReferenceClockBeforeResume(
						"before-new-system-sleep-clear-previous-generation");
				const bool previousClearRecovered =
					southChickenWakeClearArmed == 0;
				wakePanelResetAttempted = 0;
				wakeHotPlugRestoreCount = 0;
				wakeHPDReadyAfterNativePanelOn = 0;
				wakeRawClockRestoreAttempted = 0;
				auto lock = southChickenLock;
				if (lock != nullptr)
					IOLockLock(lock);
				southChickenPanelOffCompleted = 0;
				southChickenSuspendSetArmed =
					previousClearRecovered &&
					targetCMLIGPUValidated != 0 &&
					v61PatchFullyArmed != 0 ? 1 : 0;
				southChickenDurationRestoreArmed =
					previousClearRecovered &&
					targetCMLIGPUValidated != 0 &&
					v61PatchFullyArmed != 0 ? 1 : 0;
				southChickenDurationRestoreAttempted = 0;
				southChickenDurationRestoreWriteIssued = 0;
				southChickenDurationRestoreVerified = 0;
				southChickenDurationFirstTrainingSampled = 0;
				if (previousClearRecovered) {
					southChickenSleepSetAttempted = 0;
					southChickenSleepSetWriteIssued = 0;
					southChickenSleepSetVerified = 0;
					southChickenWakeClearArmed = 0;
					southChickenWakeClearAttempted = 0;
					southChickenWakeClearVerified = 0;
				}
				southChickenResumeBoundaryObserved = 0;
				// Publish the committed generation only after every CNP state field
				// has been reset.  The IGPU power-interest callback and FB0 work loop
				// run on different threads.
				OSSynchronizeIO();
				systemSleepPending = 1;
				if (lock != nullptr)
					IOLockUnlock(lock);
				cmlPhyReinitArmed = previousClearRecovered &&
					targetCMLIGPUValidated != 0 &&
					v61PatchFullyArmed != 0 ? 1 : 0;
				cmlPhyReinitAttempted = 0;
				// Close the only cross-notifier race.  The priority callback first
				// makes termination sticky and then disarms these same scalar gates.
				// If it ran at any point during preparation, roll the ordinary sleep
				// generation back after its final commit.  If it runs after this
				// check, its own disarm wins instead.
				OSSynchronizeIO();
				if (systemTerminationPending != 0) {
					if (lock != nullptr)
						IOLockLock(lock);
					systemSleepPending = 0;
					southChickenSuspendSetArmed = 0;
					southChickenDurationRestoreArmed = 0;
					southChickenPanelOffCompleted = 0;
					cmlPhyReinitArmed = 0;
					panelNeedsD0Precondition = 0;
					forceRazerPanelPowerConfig = 0;
					OSSynchronizeIO();
					if (lock != nullptr)
						IOLockUnlock(lock);
					break;
				}
				if (RazerOLEDWakeFix_selfInstance != nullptr) {
					RazerOLEDWakeFix_selfInstance->setProperty(
						"PreviousSouthChickenSequenceStatus",
						!previousClearWasArmed ? "no-previous-clear-pending" :
						(previousClearRecovered ?
						"previous-clear-recovered-before-new-generation" :
						"previous-clear-failed-new-suspend-set-refused"));
					RazerOLEDWakeFix_selfInstance->setProperty("SystemSleepPhase",
						previousClearRecovered ?
						"will-sleep-clean-panel-off-and-igpu-late-set-armed" :
						"will-sleep-previous-refclk-clear-pending-new-set-refused");
				}
				SYSLOG("power", "system sleep committed; previous-clear-armed=%u recovered=%u, native clean panel-off=%u, IGPU WillPowerOff(state0) refclk-set=%u",
					previousClearWasArmed, previousClearRecovered,
					cmlPhyReinitArmed, southChickenSuspendSetArmed);
				break;
			}
		case kIOMessageSystemWillNotSleep:
			{
				// A termination transition is not a cancelled sleep.  Leave its
				// one-way gate armed until the machine powers off or restarts.
				if (systemTerminationPending != 0)
					break;
				auto lock = southChickenLock;
				if (lock != nullptr)
					IOLockLock(lock);
				southChickenSuspendSetArmed = 0;
				southChickenDurationRestoreArmed = 0;
				OSSynchronizeIO();
				systemSleepPending = 0;
				if (lock != nullptr)
					IOLockUnlock(lock);
			}
			cmlPhyReinitArmed = 0;
			// The matching IGPU kIOMessageDeviceWillNotPowerOff callback performs
			// the exact cancellation-time MMIO clear while BAR0 is still available.
			// Preserve a failed clear here so it can be retried safely later.
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty("SystemSleepPhase",
					southChickenWakeClearArmed != 0 ?
					"sleep-cancelled-refclk-clear-deferred-to-display-workloop" :
					"sleep-cancelled-before-refclk-latch");
			break;
		case kIOMessageSystemWillPowerOn:
			{
				auto lock = southChickenLock;
				if (lock != nullptr)
					IOLockLock(lock);
				southChickenSuspendSetArmed = 0;
				OSSynchronizeIO();
				systemSleepPending = 0;
				if (lock != nullptr)
					IOLockUnlock(lock);
			}
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty("SystemSleepPhase", "system-will-power-on");
			break;
		case kIOMessageSystemHasPoweredOn:
			{
				auto lock = southChickenLock;
				if (lock != nullptr)
					IOLockLock(lock);
				southChickenSuspendSetArmed = 0;
				OSSynchronizeIO();
				systemSleepPending = 0;
				if (lock != nullptr)
					IOLockUnlock(lock);
			}
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty("SystemSleepPhase",
					southChickenWakeClearArmed != 0 ?
					"system-awake-refclk-clear-still-pending" :
					"system-awake");
			break;
		default:
			break;
	}
	return kIOReturnSuccess;
}

static void logPanelPowerRegisters(void *controller, const char *stage) {
	if (callReadRegister32 == nullptr)
		return;

	SYSLOG("power", "%s: PP_STATUS=0x%08X PP_CONTROL=0x%08X PP_ON_DELAYS=0x%08X PP_OFF_DELAYS=0x%08X PP_DIVISOR=0x%08X",
		stage,
		callReadRegister32(controller, kPanelPowerStatus),
		callReadRegister32(controller, kPanelPowerControl),
		callReadRegister32(controller, kPanelPowerOnDelays),
		callReadRegister32(controller, kPanelPowerOffDelays),
		callReadRegister32(controller, kPanelPowerDivisor));
}

static void publishRegister32(const char *name, uint32_t value) {
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty(name,
			static_cast<uint64_t>(value), 32);
}

enum class PanelPowerSnapshotStage : uint32_t {
	WakeResetInitial,
	WakeResetFirstOn,
	WakeResetAfterD3Delay,
	WakeResetAfterOff,
	WakeResetAfterOn
};

static void publishPanelPowerSnapshot(void *controller,
	PanelPowerSnapshotStage stage) {
	if (callReadRegister32 == nullptr)
		return;

	const char *statusName = nullptr;
	const char *controlName = nullptr;
	const char *onDelaysName = nullptr;
	const char *offDelaysName = nullptr;
	const char *divisorName = nullptr;
	switch (stage) {
		case PanelPowerSnapshotStage::WakeResetInitial:
			statusName = "WakeResetInitial_PP_STATUS";
			controlName = "WakeResetInitial_PP_CONTROL";
			onDelaysName = "WakeResetInitial_PP_ON_DELAYS";
			offDelaysName = "WakeResetInitial_PP_OFF_DELAYS";
			divisorName = "WakeResetInitial_PP_DIVISOR";
			break;
		case PanelPowerSnapshotStage::WakeResetFirstOn:
			statusName = "WakeResetFirstOn_PP_STATUS";
			controlName = "WakeResetFirstOn_PP_CONTROL";
			onDelaysName = "WakeResetFirstOn_PP_ON_DELAYS";
			offDelaysName = "WakeResetFirstOn_PP_OFF_DELAYS";
			divisorName = "WakeResetFirstOn_PP_DIVISOR";
			break;
		case PanelPowerSnapshotStage::WakeResetAfterD3Delay:
			statusName = "WakeResetAfterD3Delay_PP_STATUS";
			controlName = "WakeResetAfterD3Delay_PP_CONTROL";
			onDelaysName = "WakeResetAfterD3Delay_PP_ON_DELAYS";
			offDelaysName = "WakeResetAfterD3Delay_PP_OFF_DELAYS";
			divisorName = "WakeResetAfterD3Delay_PP_DIVISOR";
			break;
		case PanelPowerSnapshotStage::WakeResetAfterOff:
			statusName = "WakeResetAfterOff_PP_STATUS";
			controlName = "WakeResetAfterOff_PP_CONTROL";
			onDelaysName = "WakeResetAfterOff_PP_ON_DELAYS";
			offDelaysName = "WakeResetAfterOff_PP_OFF_DELAYS";
			divisorName = "WakeResetAfterOff_PP_DIVISOR";
			break;
		case PanelPowerSnapshotStage::WakeResetAfterOn:
			statusName = "WakeResetAfterOn_PP_STATUS";
			controlName = "WakeResetAfterOn_PP_CONTROL";
			onDelaysName = "WakeResetAfterOn_PP_ON_DELAYS";
			offDelaysName = "WakeResetAfterOn_PP_OFF_DELAYS";
			divisorName = "WakeResetAfterOn_PP_DIVISOR";
			break;
	}

	publishRegister32(statusName,
		callReadRegister32(controller, kPanelPowerStatus));
	publishRegister32(controlName,
		callReadRegister32(controller, kPanelPowerControl));
	publishRegister32(onDelaysName,
		callReadRegister32(controller, kPanelPowerOnDelays));
	publishRegister32(offDelaysName,
		callReadRegister32(controller, kPanelPowerOffDelays));
	publishRegister32(divisorName,
		callReadRegister32(controller, kPanelPowerDivisor));
}

static void publishRegisterData(const char *name, const uint32_t *values,
	uint32_t dwordCount) {
	if (RazerOLEDWakeFix_selfInstance != nullptr && values != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty(name,
			const_cast<uint32_t *>(values),
			dwordCount * sizeof(uint32_t));
}

// Snapshot order is fixed and published once as HPDRegisterSnapshotLayout:
// 0x44030, 0xC4000, 0xC4004, 0xC400C, 0xC4030, 0xC4038.
static void publishHPDRegisterSnapshot(void *controller,
	const char *propertyName) {
	if (callReadRegister32 == nullptr || propertyName == nullptr)
		return;

	const uint32_t values[] {
		callReadRegister32(controller, kDigitalPortHotPlugControl),
		callReadRegister32(controller, kSouthDisplayInterruptIdentity),
		callReadRegister32(controller, kSouthDisplayInterruptMask),
		callReadRegister32(controller, kSouthDisplayInterruptEnable),
		callReadRegister32(controller, kSPTHotPlugControl),
		callReadRegister32(controller, kShortHotPlugFilterCount)
	};
	publishRegisterData(propertyName, values, arrsize(values));
}

static bool restoreWindowsEDPHotPlugControls(void *controller,
	const char *stage, const char *beforeProperty, const char *afterProperty,
	const char *statusProperty) {
	if (callReadRegister32 == nullptr || callWriteRegister32 == nullptr ||
		targetCMLIGPUValidated == 0)
		return false;

	publishHPDRegisterSnapshot(controller, beforeProperty);
	const auto cpuBefore = callReadRegister32(controller,
		kDigitalPortHotPlugControl);
	const auto pchBefore = callReadRegister32(controller,
		kSPTHotPlugControl);
	const auto cpuRequested =
		(cpuBefore & ~kDigitalPortAHotPlugEnable) |
		kDigitalPortAHotPlugEnable | kDigitalPortAHotPlugStatusW1C;
	const auto pchRequested =
		(pchBefore & kSPTHotPlugWindowsPreserveMask) |
		kSPTPortAHotPlugEnable | kSPTPortAHotPlugStatusW1C;

	// Exact order used by Windows HSWEDPDDIA_SetHotPlug: CPU Port-A detector
	// first, SPT/PCH Port-A detector second.
	callWriteRegister32(controller, kDigitalPortHotPlugControl,
		cpuRequested);
	OSSynchronizeIO();
	callWriteRegister32(controller, kSPTHotPlugControl, pchRequested);
	OSSynchronizeIO();

	const auto cpuAfter = callReadRegister32(controller,
		kDigitalPortHotPlugControl);
	const auto pchAfter = callReadRegister32(controller,
		kSPTHotPlugControl);
	const auto sdeAfter = callReadRegister32(controller,
		kSouthDisplayInterruptIdentity);
	const bool verified =
		(cpuAfter & kDigitalPortAHotPlugEnable) != 0 &&
		(pchAfter & kSPTPortAHotPlugEnable) != 0;
	const auto attempt = ++wakeHotPlugRestoreCount;

	publishRegister32("WakeHPDRestoreCount", attempt);
	publishRegister32("WakeHPD_CPURequested", cpuRequested);
	publishRegister32("WakeHPD_PCHRequested", pchRequested);
	publishRegister32("WakeHPD_SDEISRImmediate", sdeAfter);
	publishHPDRegisterSnapshot(controller, afterProperty);
	if (RazerOLEDWakeFix_selfInstance != nullptr && statusProperty != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty(statusProperty,
			verified ?
			"windows-hswedpddia-dual-hpd-controls-enabled-verified" :
			"dual-hpd-control-write-verification-failed");
	SYSLOG("hpd", "%s Windows HSWEDPDDIA hot-plug restore #%u: CPU %08X->%08X requested=%08X PCH %08X->%08X requested=%08X SDEISR=%08X verified=%u",
		stage != nullptr ? stage : "unknown", attempt,
		cpuBefore, cpuAfter, cpuRequested, pchBefore, pchAfter,
		pchRequested, sdeAfter, verified);
	return verified;
}

static bool waitForPortAHPDAfterNativePanelOn(void *controller) {
	if (callReadRegister32 == nullptr)
		return false;

	uint32_t polls = 0;
	uint32_t sde = callReadRegister32(controller,
		kSouthDisplayInterruptIdentity);
	while ((sde & kSDEPortAHotPlugLive) == 0 &&
		polls < kHPDReadyPollCount) {
		IOSleep(kHPDReadyPollDelayMs);
		polls++;
		sde = callReadRegister32(controller,
			kSouthDisplayInterruptIdentity);
	}
	const bool ready = (sde & kSDEPortAHotPlugLive) != 0;
	publishRegister32("WakeNativeHPDReadyPolls", polls);
	publishRegister32("WakeNativeHPDReadySDEISR", sde);
	publishRegister32("WakeNativeHPDReady", ready ? 1 : 0);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("WakeNativeHPDReadyStatus",
			ready ? "port-a-hpd-live-after-dual-control-restore" :
				"port-a-hpd-still-low-after-2-second-bounded-wait");
	SYSLOG("hpd", "native panel-on Port-A HPD wait: ready=%u polls=%u/%u SDEISR=%08X",
		ready, polls, kHPDReadyPollCount, sde);
	return ready;
}

static bool captureDPCDSnapshot(void *controller, void *framebuffer,
	void *displayPath, DPCDSnapshotRecord *records, const char *propertyName) {
	if (callReadAUX == nullptr || records == nullptr || propertyName == nullptr ||
		!isInternalEDPPath(framebuffer, displayPath))
		return false;

	uint32_t successfulBlocks = 0;
	uint32_t successfulEssentialBlocks = 0;
	for (uint32_t index = 0; index < kDPCDSnapshotBlockCount; index++) {
		auto &record = records[index];
		record.address = kDPCDSnapshotAddresses[index];
		for (uint32_t byte = 0; byte < kDPCDSnapshotBlockSize; byte++)
			record.data[byte] = 0;
		const uint16_t readLength = record.address == 0x0600 ? 1 :
			static_cast<uint16_t>(kDPCDSnapshotBlockSize);
		record.result = static_cast<uint32_t>(callReadAUX(controller,
			framebuffer, record.address, readLength,
			record.data, displayPath));
		if (record.result == kIOReturnSuccess) {
			successfulBlocks++;
			// These four blocks are sufficient to compare the receiver's
			// advertised capability, active link configuration, training status,
			// and D0/D3 power state.  The remaining diagnostic blocks are optional
			// on older eDP revisions and must not invalidate an otherwise useful
			// cold snapshot.
			if (record.address == 0x0000 || record.address == 0x0100 ||
				record.address == 0x0200 || record.address == 0x0600)
				successfulEssentialBlocks++;
		}
	}

	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty(propertyName, records,
			sizeof(DPCDSnapshotRecord) * kDPCDSnapshotBlockCount);
		RazerOLEDWakeFix_selfInstance->setProperty(
			"DPCDSnapshotLastSuccessfulBlocks",
			static_cast<uint64_t>(successfulBlocks), 32);
		RazerOLEDWakeFix_selfInstance->setProperty(
			"DPCDSnapshotLastSuccessfulEssentialBlocks",
			static_cast<uint64_t>(successfulEssentialBlocks), 32);
	}
	SYSLOG("dpcd", "%s captured %u/%u targeted blocks; essential=%u/4",
		propertyName, successfulBlocks, kDPCDSnapshotBlockCount,
		successfulEssentialBlocks);
	return successfulEssentialBlocks == 4;
}

static bool coldWorkingTrainingIntervalMatchesEvidence() {
	if (savedColdDPCDValid == 0)
		return false;

	for (uint32_t index = 0; index < kDPCDSnapshotBlockCount; index++) {
		const auto &record = savedColdDPCD[index];
		if (record.address != 0x0000 ||
			record.result != kIOReturnSuccess)
			continue;
		// TRAINING_AUX_RD_INTERVAL is byte 0x0E in the receiver-capability
		// block beginning at DPCD 0x0000.
		return record.data[0x0E] == kSamsungTrainingAuxReadInterval;
	}
	return false;
}

static void compareWakeDPCDWithCold(const DPCDSnapshotRecord *wake,
	const char *stage) {
	if (wake == nullptr || savedColdDPCDValid == 0)
		return;

	uint32_t changedBlocks = 0;
	uint32_t changedBytes = 0;
	uint32_t unavailableBlocks = 0;
	for (uint32_t index = 0; index < kDPCDSnapshotBlockCount; index++) {
		if (savedColdDPCD[index].result != kIOReturnSuccess ||
			wake[index].result != kIOReturnSuccess) {
			unavailableBlocks++;
			continue;
		}
		bool blockChanged = false;
		for (uint32_t byte = 0; byte < kDPCDSnapshotBlockSize; byte++) {
			if (savedColdDPCD[index].data[byte] != wake[index].data[byte]) {
				changedBytes++;
				blockChanged = true;
			}
		}
		if (blockChanged)
			changedBlocks++;
	}

	publishRegister32("WakeDPCDChangedBlocksVsCold", changedBlocks);
	publishRegister32("WakeDPCDChangedBytesVsCold", changedBytes);
	publishRegister32("WakeDPCDUnavailableBlocks", unavailableBlocks);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("WakeDPCDComparisonStage",
			stage);
	SYSLOG("dpcd", "%s vs cold: changed-blocks=%u changed-bytes=%u unavailable=%u",
		stage, changedBlocks, changedBytes, unavailableBlocks);
}

static void readDDIBufferTranslation(void *controller, uint32_t *values) {
	if (callReadRegister32 == nullptr || values == nullptr)
		return;

	for (uint32_t index = 0; index < kDDIBufferTranslationDwordCount; index++)
		values[index] = callReadRegister32(controller,
			kDDIBufferTranslationA + index * sizeof(uint32_t));
}

static bool captureDDIBufferTranslation(void *controller,
	const char *source) {
	if (callReadRegister32 == nullptr)
		return false;

	uint32_t captured[kDDIBufferTranslationDwordCount] {};
	readDDIBufferTranslation(controller, captured);
	for (uint32_t index = 0; index < kDDIBufferTranslationDwordCount; index++)
		savedDDIBufferTranslation[index] = captured[index];
	OSSynchronizeIO();
	savedDDIBufferTranslationValid = 1;

	publishRegisterData("Saved_DDI_BUF_TRANS_A", savedDDIBufferTranslation,
		kDDIBufferTranslationDwordCount);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty("DDITranslationCaptureSource",
			source);
		RazerOLEDWakeFix_selfInstance->setProperty("DDITranslationRestoreStatus",
			"captured-not-yet-restored");
	}

	SYSLOG("ddi", "%s DDI_BUF_TRANS_A captured: 0=%08X/%08X 1=%08X/%08X 2=%08X/%08X",
		source, captured[0], captured[1], captured[2], captured[3],
		captured[4], captured[5]);
	SYSLOG("ddi", "%s DDI_BUF_TRANS_A captured: 3=%08X/%08X 4=%08X/%08X 5=%08X/%08X",
		source, captured[6], captured[7], captured[8], captured[9],
		captured[10], captured[11]);
	SYSLOG("ddi", "%s DDI_BUF_TRANS_A captured: 6=%08X/%08X 7=%08X/%08X 8=%08X/%08X",
		source, captured[12], captured[13], captured[14], captured[15],
		captured[16], captured[17]);
	return true;
}

static bool restoreDDIBufferTranslation(void *controller) {
	if (callReadRegister32 == nullptr || callWriteRegister32 == nullptr ||
		savedDDIBufferTranslationValid == 0)
		return false;

	uint32_t before[kDDIBufferTranslationDwordCount] {};
	uint32_t after[kDDIBufferTranslationDwordCount] {};
	readDDIBufferTranslation(controller, before);
	uint32_t changed = 0;
	for (uint32_t index = 0; index < kDDIBufferTranslationDwordCount; index++) {
		if (before[index] != savedDDIBufferTranslation[index])
			changed++;
		callWriteRegister32(controller,
			kDDIBufferTranslationA + index * sizeof(uint32_t),
			savedDDIBufferTranslation[index]);
	}
	OSSynchronizeIO();
	readDDIBufferTranslation(controller, after);

	bool verified = true;
	for (uint32_t index = 0; index < kDDIBufferTranslationDwordCount; index++) {
		if (after[index] != savedDDIBufferTranslation[index]) {
			verified = false;
			break;
		}
	}

	publishRegisterData("WakeBeforeRestore_DDI_BUF_TRANS_A", before,
		kDDIBufferTranslationDwordCount);
	publishRegisterData("WakeAfterRestore_DDI_BUF_TRANS_A", after,
		kDDIBufferTranslationDwordCount);
	publishRegister32("WakeTranslationChangedDwords", changed);
	publishRegister32("WakeTranslationRestoreAttempts",
		++wakeTranslationRestoreAttempts);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("DDITranslationRestoreStatus",
			verified ? "restored-verified" : "restore-verification-failed");

	SYSLOG("ddi", "wake DDI_BUF_TRANS_A exact restore: changed=%u after=%08X/%08X,%08X/%08X,%08X/%08X verified=%u attempt=%u",
		changed, after[0], after[1], after[2], after[3], after[4], after[5],
		verified, wakeTranslationRestoreAttempts);
	return verified;
}

enum class PSRSnapshotStage : uint32_t {
	ColdWorking,
	SleepPre,
	WakePre,
	WakeAfterExit,
	WakePost
};

static void publishPSRRegisterSnapshot(void *controller,
	PSRSnapshotStage stage) {
	if (callReadRegister32 == nullptr ||
		RazerOLEDWakeFix_selfInstance == nullptr)
		return;

	const auto psrControl = callReadRegister32(controller, kEDPPSRControl);
	const auto psrStatus = callReadRegister32(controller, kEDPPSRStatus);
	const auto psrEvent = callReadRegister32(controller, kEDPPSREvent);
	const auto psrDebug = callReadRegister32(controller, kEDPPSRDebug);
	const auto psr2Control = callReadRegister32(controller, kEDPPSR2Control);
	const auto psr2Status = callReadRegister32(controller, kEDPPSR2Status);
	const auto pipeAStatus = callReadRegister32(controller, kPipeAStatus);

	switch (stage) {
		case PSRSnapshotStage::ColdWorking:
			publishRegister32("ColdWorking_PSR_CTL", psrControl);
			publishRegister32("ColdWorking_PSR_STATUS", psrStatus);
			publishRegister32("ColdWorking_PSR_EVENT", psrEvent);
			publishRegister32("ColdWorking_PSR_DEBUG", psrDebug);
			publishRegister32("ColdWorking_PSR2_CTL", psr2Control);
			publishRegister32("ColdWorking_PSR2_STATUS", psr2Status);
			publishRegister32("ColdWorking_PIPEA_STATUS", pipeAStatus);
			break;
		case PSRSnapshotStage::SleepPre:
			publishRegister32("SleepPre_PSR_CTL", psrControl);
			publishRegister32("SleepPre_PSR_STATUS", psrStatus);
			publishRegister32("SleepPre_PSR_EVENT", psrEvent);
			publishRegister32("SleepPre_PSR_DEBUG", psrDebug);
			publishRegister32("SleepPre_PSR2_CTL", psr2Control);
			publishRegister32("SleepPre_PSR2_STATUS", psr2Status);
			publishRegister32("SleepPre_PIPEA_STATUS", pipeAStatus);
			break;
		case PSRSnapshotStage::WakePre:
			publishRegister32("WakePre_PSR_CTL", psrControl);
			publishRegister32("WakePre_PSR_STATUS", psrStatus);
			publishRegister32("WakePre_PSR_EVENT", psrEvent);
			publishRegister32("WakePre_PSR_DEBUG", psrDebug);
			publishRegister32("WakePre_PSR2_CTL", psr2Control);
			publishRegister32("WakePre_PSR2_STATUS", psr2Status);
			publishRegister32("WakePre_PIPEA_STATUS", pipeAStatus);
			break;
		case PSRSnapshotStage::WakeAfterExit:
			publishRegister32("WakeAfterPSRExit_PSR_CTL", psrControl);
			publishRegister32("WakeAfterPSRExit_PSR_STATUS", psrStatus);
			publishRegister32("WakeAfterPSRExit_PSR_EVENT", psrEvent);
			publishRegister32("WakeAfterPSRExit_PSR_DEBUG", psrDebug);
			publishRegister32("WakeAfterPSRExit_PSR2_CTL", psr2Control);
			publishRegister32("WakeAfterPSRExit_PSR2_STATUS", psr2Status);
			publishRegister32("WakeAfterPSRExit_PIPEA_STATUS", pipeAStatus);
			break;
		case PSRSnapshotStage::WakePost:
			publishRegister32("WakePost_PSR_CTL", psrControl);
			publishRegister32("WakePost_PSR_STATUS", psrStatus);
			publishRegister32("WakePost_PSR_EVENT", psrEvent);
			publishRegister32("WakePost_PSR_DEBUG", psrDebug);
			publishRegister32("WakePost_PSR2_CTL", psr2Control);
			publishRegister32("WakePost_PSR2_STATUS", psr2Status);
			publishRegister32("WakePost_PIPEA_STATUS", pipeAStatus);
			break;
	}
}

static void publishDisplayRegisterSnapshot(void *controller, const char *stage) {
	if (callReadRegister32 == nullptr || RazerOLEDWakeFix_selfInstance == nullptr)
		return;

	// Property names are intentionally fixed: no formatting or allocation is
	// performed in the graphics driver's serialized power path.
	if (stage[0] == 'S') {
		publishPSRRegisterSnapshot(controller, PSRSnapshotStage::SleepPre);
		publishRegister32("SleepPre_BMU", callReadRegister32(controller, kDisplayIOBalanceLeg));
		publishRegister32("SleepPre_DDI_BUF_CTL_A", callReadRegister32(controller, kDDIBufferControlA));
		publishRegister32("SleepPre_DP_TP_CTL_A", callReadRegister32(controller, kDPTransportControlA));
		publishRegister32("SleepPre_DP_TP_STATUS_A", callReadRegister32(controller, kDPTransportStatusA));
		publishRegister32("SleepPre_PORT_CLK_SEL_A", callReadRegister32(controller, kPortClockSelectA));
		publishRegister32("SleepPre_DPLL_CTRL1", callReadRegister32(controller, kDPLLControl1));
		publishRegister32("SleepPre_DPLL_CTRL2", callReadRegister32(controller, kDPLLControl2));
		publishRegister32("SleepPre_DPLL_STATUS", callReadRegister32(controller, kDPLLStatus));
		publishRegister32("SleepPre_LCPLL1_CTL", callReadRegister32(controller, kLCPLL1Control));
		publishRegister32("SleepPre_TRANS_DDI_A", callReadRegister32(controller, kTransDDIFunctionControlA));
		publishRegister32("SleepPre_TRANS_DDI_EDP", callReadRegister32(controller, kTransDDIFunctionControlEDP));
		publishRegister32("SleepPre_CHICKEN_TRANS_EDP", callReadRegister32(controller, kChickenTransEDP));
		publishRegister32("SleepPre_PWR_WELL_CTL2", callReadRegister32(controller, kPowerWellControl2));
		publishRegister32("SleepPre_FUSE_STATUS", callReadRegister32(controller, kDisplayFuseStatus));
		publishRegister32("SleepPre_DBUF_CTL", callReadRegister32(controller, kDisplayBufferControl));
	} else if (stage[5] == 'r') {
		publishPSRRegisterSnapshot(controller, PSRSnapshotStage::WakePre);
		publishRegister32("WakePre_BMU", callReadRegister32(controller, kDisplayIOBalanceLeg));
		publishRegister32("WakePre_DDI_BUF_CTL_A", callReadRegister32(controller, kDDIBufferControlA));
		publishRegister32("WakePre_DP_TP_CTL_A", callReadRegister32(controller, kDPTransportControlA));
		publishRegister32("WakePre_DP_TP_STATUS_A", callReadRegister32(controller, kDPTransportStatusA));
		publishRegister32("WakePre_PORT_CLK_SEL_A", callReadRegister32(controller, kPortClockSelectA));
		publishRegister32("WakePre_DPLL_CTRL1", callReadRegister32(controller, kDPLLControl1));
		publishRegister32("WakePre_DPLL_CTRL2", callReadRegister32(controller, kDPLLControl2));
		publishRegister32("WakePre_DPLL_STATUS", callReadRegister32(controller, kDPLLStatus));
		publishRegister32("WakePre_LCPLL1_CTL", callReadRegister32(controller, kLCPLL1Control));
		publishRegister32("WakePre_TRANS_DDI_A", callReadRegister32(controller, kTransDDIFunctionControlA));
		publishRegister32("WakePre_TRANS_DDI_EDP", callReadRegister32(controller, kTransDDIFunctionControlEDP));
		publishRegister32("WakePre_CHICKEN_TRANS_EDP", callReadRegister32(controller, kChickenTransEDP));
		publishRegister32("WakePre_PWR_WELL_CTL2", callReadRegister32(controller, kPowerWellControl2));
		publishRegister32("WakePre_FUSE_STATUS", callReadRegister32(controller, kDisplayFuseStatus));
		publishRegister32("WakePre_DBUF_CTL", callReadRegister32(controller, kDisplayBufferControl));
	} else {
		publishPSRRegisterSnapshot(controller, PSRSnapshotStage::WakePost);
		publishRegister32("WakePost_BMU", callReadRegister32(controller, kDisplayIOBalanceLeg));
		publishRegister32("WakePost_DDI_BUF_CTL_A", callReadRegister32(controller, kDDIBufferControlA));
		publishRegister32("WakePost_DP_TP_CTL_A", callReadRegister32(controller, kDPTransportControlA));
		publishRegister32("WakePost_DP_TP_STATUS_A", callReadRegister32(controller, kDPTransportStatusA));
		publishRegister32("WakePost_PORT_CLK_SEL_A", callReadRegister32(controller, kPortClockSelectA));
		publishRegister32("WakePost_DPLL_CTRL1", callReadRegister32(controller, kDPLLControl1));
		publishRegister32("WakePost_DPLL_CTRL2", callReadRegister32(controller, kDPLLControl2));
		publishRegister32("WakePost_DPLL_STATUS", callReadRegister32(controller, kDPLLStatus));
		publishRegister32("WakePost_LCPLL1_CTL", callReadRegister32(controller, kLCPLL1Control));
		publishRegister32("WakePost_TRANS_DDI_A", callReadRegister32(controller, kTransDDIFunctionControlA));
		publishRegister32("WakePost_TRANS_DDI_EDP", callReadRegister32(controller, kTransDDIFunctionControlEDP));
		publishRegister32("WakePost_CHICKEN_TRANS_EDP", callReadRegister32(controller, kChickenTransEDP));
		publishRegister32("WakePost_PWR_WELL_CTL2", callReadRegister32(controller, kPowerWellControl2));
		publishRegister32("WakePost_FUSE_STATUS", callReadRegister32(controller, kDisplayFuseStatus));
		publishRegister32("WakePost_DBUF_CTL", callReadRegister32(controller, kDisplayBufferControl));
	}
}

static bool exitPSRBeforeWakeLinkTraining(void *controller,
	void *framebuffer, void *displayPath) {
	if (callReadRegister32 == nullptr || callWriteRegister32 == nullptr ||
		callReadAUX == nullptr || callWriteAUX == nullptr ||
		!isInternalEDPPath(framebuffer, displayPath)) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("WakePSRExitStatus",
				"refused-missing-symbol-or-non-internal-edp");
		return false;
	}

	const auto psrControlBefore = callReadRegister32(controller,
		kEDPPSRControl);
	const auto psrStatusBefore = callReadRegister32(controller,
		kEDPPSRStatus);
	const auto psr2ControlBefore = callReadRegister32(controller,
		kEDPPSR2Control);
	const auto psr2StatusBefore = callReadRegister32(controller,
		kEDPPSR2Status);
	publishRegister32("WakePSRExitBefore_PSR_CTL", psrControlBefore);
	publishRegister32("WakePSRExitBefore_PSR_STATUS", psrStatusBefore);
	publishRegister32("WakePSRExitBefore_PSR2_CTL", psr2ControlBefore);
	publishRegister32("WakePSRExitBefore_PSR2_STATUS", psr2StatusBefore);

	// Intel disables PSR2 before PSR1.  The Camellia value is copied from the
	// exact Tahoe 25.2 framebuffer binary and has both PSR enable bits clear.
	callWriteRegister32(controller, kEDPPSR2Control,
		psr2ControlBefore & ~kPSREnable);
	OSSynchronizeIO();
	callWriteRegister32(controller, kEDPPSRControl,
		kAppleCamelliaPSRDisabledControl);
	OSSynchronizeIO();

	uint32_t psrStatusAfter = psrStatusBefore;
	uint32_t psr2StatusAfter = psr2StatusBefore;
	uint32_t pollCount = 0;
	for (; pollCount < kPSRExitPollCount; pollCount++) {
		psrStatusAfter = callReadRegister32(controller, kEDPPSRStatus);
		psr2StatusAfter = callReadRegister32(controller, kEDPPSR2Status);
		if ((psrStatusAfter & kPSRStatusStateMask) == 0 &&
			(psr2StatusAfter & kPSR2StatusStateMask) == 0)
			break;
		IOSleep(kPSRExitPollDelayMs);
	}
	const bool sourceIdle =
		(psrStatusAfter & kPSRStatusStateMask) == 0 &&
		(psr2StatusAfter & kPSR2StatusStateMask) == 0;

	uint8_t sinkPSRSupport = 0;
	uint8_t sinkPSRConfigBefore = 0xFF;
	uint8_t sinkPSRConfigAfter = 0xFF;
	uint8_t sinkPSRDisabled = 0;
	const auto supportResult = callReadAUX(controller, framebuffer,
		kDPCDPSRSupport, 1, &sinkPSRSupport, displayPath);
	const auto configReadBeforeResult = callReadAUX(controller, framebuffer,
		kDPCDPSREnableConfig, 1, &sinkPSRConfigBefore, displayPath);
	IOReturn configWriteResult = kIOReturnNotReady;
	IOReturn configReadAfterResult = kIOReturnNotReady;
	if (configReadBeforeResult == kIOReturnSuccess) {
		if (sinkPSRConfigBefore != 0) {
			configWriteResult = callWriteAUX(controller, framebuffer,
				kDPCDPSREnableConfig, 1, &sinkPSRDisabled, displayPath);
		} else {
			configWriteResult = kIOReturnSuccess;
		}
		configReadAfterResult = callReadAUX(controller, framebuffer,
			kDPCDPSREnableConfig, 1, &sinkPSRConfigAfter, displayPath);
	}
	const bool sinkDisabled = configReadBeforeResult == kIOReturnSuccess &&
		configWriteResult == kIOReturnSuccess &&
		configReadAfterResult == kIOReturnSuccess && sinkPSRConfigAfter == 0;

	publishRegister32("WakePSRSupportReadResult",
		static_cast<uint32_t>(supportResult));
	publishRegister32("WakePSRSupport", sinkPSRSupport);
	publishRegister32("WakePSRConfigReadBeforeResult",
		static_cast<uint32_t>(configReadBeforeResult));
	publishRegister32("WakePSRConfigBefore", sinkPSRConfigBefore);
	publishRegister32("WakePSRConfigWriteResult",
		static_cast<uint32_t>(configWriteResult));
	publishRegister32("WakePSRConfigReadAfterResult",
		static_cast<uint32_t>(configReadAfterResult));
	publishRegister32("WakePSRConfigAfter", sinkPSRConfigAfter);
	publishRegister32("WakePSRExitPollCount", pollCount);
	publishRegister32("WakePSRExitAfter_PSR_CTL",
		callReadRegister32(controller, kEDPPSRControl));
	publishRegister32("WakePSRExitAfter_PSR_STATUS", psrStatusAfter);
	publishRegister32("WakePSRExitAfter_PSR2_CTL",
		callReadRegister32(controller, kEDPPSR2Control));
	publishRegister32("WakePSRExitAfter_PSR2_STATUS", psr2StatusAfter);
	publishPSRRegisterSnapshot(controller, PSRSnapshotStage::WakeAfterExit);

	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty("WakePSRSourceExitStatus",
			sourceIdle ? "source-psr1-psr2-idle" :
				"source-psr-state-timeout");
		RazerOLEDWakeFix_selfInstance->setProperty("WakePSRSinkDisableStatus",
			sinkDisabled ? "sink-dpcd-0170-disabled" :
				"sink-dpcd-0170-disable-failed-or-unavailable");
		RazerOLEDWakeFix_selfInstance->setProperty("WakePSRExitStatus",
			sourceIdle && sinkDisabled ?
				"source-idle-and-sink-disabled-before-link-training" :
				"psr-exit-incomplete-before-link-training");
	}
	SYSLOG("psr", "S3 PSR exit before link training: source CTL=%08X STATUS=%08X PSR2_CTL=%08X PSR2_STATUS=%08X -> STATUS=%08X PSR2_STATUS=%08X idle=%u polls=%u; sink support result=0x%08X value=%02X config %02X->%02X write=0x%08X disabled=%u",
		psrControlBefore, psrStatusBefore, psr2ControlBefore,
		psr2StatusBefore, psrStatusAfter, psr2StatusAfter, sourceIdle,
		pollCount, static_cast<uint32_t>(supportResult), sinkPSRSupport,
		sinkPSRConfigBefore, sinkPSRConfigAfter,
		static_cast<uint32_t>(configWriteResult), sinkDisabled);
	return sourceIdle && sinkDisabled;
}

// Replacement target for only Tahoe 25.2 LinkTraining's initial native
// WriteAUX(0x100, 2) call.  The target panel advertises eDP 1.4's supported
// link-rate table, and both known-working non-Apple drivers select its HBR2
// entry through 0x115 rather than writing the legacy HBR2 code to 0x100.
// Every condition is revalidated against the live call and live sink.  A
// mismatch or failed AUX operation returns to Apple's exact native write.
static uint16_t decodeSupportedLinkRate20KHz(const uint8_t *bytes) {
	if (bytes == nullptr)
		return 0;
	return static_cast<uint16_t>(static_cast<uint32_t>(bytes[0]) |
		(static_cast<uint32_t>(bytes[1]) << 8));
}

static IOReturn writeEDP14RateSelectAtNativeLinkTrainingCall(
	void *controller, void *framebuffer, uint32_t address, uint16_t length,
	void *buffer, void *displayPath) {
	if (callWriteAUX == nullptr)
		return kIOReturnNotReady;

	const auto nativeBytes = static_cast<const uint8_t *>(buffer);
	const uint8_t nativeBandwidth = nativeBytes != nullptr && length >= 1 ?
		nativeBytes[0] : static_cast<uint8_t>(0xFF);
	const uint8_t nativeLaneCount = nativeBytes != nullptr && length >= 2 ?
		nativeBytes[1] : static_cast<uint8_t>(0xFF);
	const bool boundedS3EDPCall =
		trainingDiagnosticsArmed != 0 &&
		panelNeedsD0Precondition != 0 &&
		targetCMLIGPUValidated != 0 &&
		v61PatchFullyArmed != 0 &&
		systemTerminationPending == 0 &&
		isInternalEDPPath(framebuffer, displayPath) &&
		address == kDPCDLinkBandwidthSet && length == 2 &&
		nativeBytes != nullptr;
	if (!boundedS3EDPCall)
		return callWriteAUX(controller, framebuffer, address, length, buffer,
			displayPath);

	const auto count = ++nativeLinkRateSelectInterceptCount;
	const bool nativeConfigMatches =
		nativeBandwidth == kAppleHBR2LegacyBandwidth &&
		nativeLaneCount == kAppleFourLaneEnhanced;
	uint8_t supportedRates[16] {};
	uint8_t oldRateSelect = 0xFF;
	uint8_t requestedRateSelect = 0xFF;
	IOReturn supportedRatesReadResult = kIOReturnNotReady;
	IOReturn rateSelectReadResult = kIOReturnNotReady;
	IOReturn rateSelectWriteResult = kIOReturnNotReady;
	IOReturn laneCountWriteResult = kIOReturnNotReady;
	IOReturn rateSelectRestoreResult = kIOReturnNotReady;
	IOReturn nativeFallbackResult = kIOReturnNotReady;
	bool supportedRatesMatch = false;
	const char *status = "native-forwarded-link-config-was-not-exact-hbr2-x4";

	if (nativeConfigMatches && callReadAUX != nullptr) {
		supportedRatesReadResult = callReadAUX(controller, framebuffer,
			kDPCDSupportedLinkRates,
			static_cast<uint16_t>(sizeof(supportedRates)), supportedRates,
			displayPath);
		const uint16_t rate0 = decodeSupportedLinkRate20KHz(
			&supportedRates[0]);
		const uint16_t rate1 = decodeSupportedLinkRate20KHz(
			&supportedRates[2]);
		const uint16_t rate2 = decodeSupportedLinkRate20KHz(
			&supportedRates[4]);
		bool unusedEntriesClear = true;
		for (size_t byte = 6; byte < sizeof(supportedRates); byte++) {
			if (supportedRates[byte] != 0) {
				unusedEntriesClear = false;
				break;
			}
		}
		supportedRatesMatch =
			supportedRatesReadResult == kIOReturnSuccess &&
			rate0 == kSupportedRateRBR20KHz &&
			rate1 == kSupportedRateHBR20KHz &&
			rate2 == kSupportedRateHBR220KHz && unusedEntriesClear;
		if (supportedRatesMatch) {
			rateSelectReadResult = callReadAUX(controller, framebuffer,
				kDPCDLinkRateSet, 1, &oldRateSelect, displayPath);
			if (rateSelectReadResult == kIOReturnSuccess) {
				requestedRateSelect = static_cast<uint8_t>(
					(oldRateSelect & ~kLinkRateSelectorMask) |
					kWindowsLinuxHBR2RateSelector);
				rateSelectWriteResult = callWriteAUX(controller, framebuffer,
					kDPCDLinkRateSet, 1, &requestedRateSelect, displayPath);
				if (rateSelectWriteResult == kIOReturnSuccess) {
					uint8_t laneCount = nativeLaneCount;
					laneCountWriteResult = callWriteAUX(controller, framebuffer,
						kDPCDLaneCountSet, 1, &laneCount, displayPath);
				}
			}
		}
	}

	IOReturn result = kIOReturnSuccess;
	if (!nativeConfigMatches || !supportedRatesMatch ||
		rateSelectReadResult != kIOReturnSuccess ||
		rateSelectWriteResult != kIOReturnSuccess ||
		laneCountWriteResult != kIOReturnSuccess) {
		// If selector 2 was accepted but the separate lane-count write failed,
		// restore 0x115 before falling back to Tahoe's legacy two-byte write.
		if (rateSelectWriteResult == kIOReturnSuccess &&
			laneCountWriteResult != kIOReturnSuccess)
			rateSelectRestoreResult = callWriteAUX(controller, framebuffer,
				kDPCDLinkRateSet, 1, &oldRateSelect, displayPath);
		nativeFallbackResult = callWriteAUX(controller, framebuffer, address,
			length, buffer, displayPath);
		result = nativeFallbackResult;
		if (!nativeConfigMatches)
			status = "native-forwarded-link-config-was-not-exact-hbr2-x4";
		else if (callReadAUX == nullptr)
			status = "native-fallback-readaux-symbol-unavailable";
		else if (!supportedRatesMatch)
			status = "native-fallback-supported-link-rate-table-mismatch";
		else if (rateSelectReadResult != kIOReturnSuccess)
			status = "native-fallback-link-rate-set-read-failed";
		else if (rateSelectWriteResult != kIOReturnSuccess)
			status = "native-fallback-link-rate-set-write-failed";
		else
			status = "native-fallback-lane-count-write-failed";
	} else {
		status = "edp14-hbr2-rate-selector-two-and-x4-lane-count-applied";
	}

	const uint16_t rate0 = decodeSupportedLinkRate20KHz(&supportedRates[0]);
	const uint16_t rate1 = decodeSupportedLinkRate20KHz(&supportedRates[2]);
	const uint16_t rate2 = decodeSupportedLinkRate20KHz(&supportedRates[4]);
	publishRegister32("WakeRateSelectInterceptCount", count);
	publishRegister32("WakeRateSelectNativeLinkConfig",
		static_cast<uint32_t>(nativeBandwidth) |
		(static_cast<uint32_t>(nativeLaneCount) << 8));
	publishRegister32("WakeRateSelectSupportedRatesReadResult",
		static_cast<uint32_t>(supportedRatesReadResult));
	publishRegister32("WakeRateSelectSupportedRate0_20KHz", rate0);
	publishRegister32("WakeRateSelectSupportedRate1_20KHz", rate1);
	publishRegister32("WakeRateSelectSupportedRate2_20KHz", rate2);
	publishRegister32("WakeRateSelectSupportedRatesVerified",
		supportedRatesMatch ? 1 : 0);
	publishRegister32("WakeRateSelectBeforeReadResult",
		static_cast<uint32_t>(rateSelectReadResult));
	publishRegister32("WakeRateSelectBefore", oldRateSelect);
	publishRegister32("WakeRateSelectRequested", requestedRateSelect);
	publishRegister32("WakeRateSelectWriteResult",
		static_cast<uint32_t>(rateSelectWriteResult));
	publishRegister32("WakeRateSelectLaneCountWriteResult",
		static_cast<uint32_t>(laneCountWriteResult));
	publishRegister32("WakeRateSelectRestoreResult",
		static_cast<uint32_t>(rateSelectRestoreResult));
	publishRegister32("WakeRateSelectNativeFallbackResult",
		static_cast<uint32_t>(nativeFallbackResult));
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeRateSelectSupportedRates", supportedRates,
			sizeof(supportedRates));
		RazerOLEDWakeFix_selfInstance->setProperty("WakeRateSelectStatus",
			status);
	}
	SYSLOG("dpcd", "native LinkTraining link-config intercept #%u: native=%02X,%02X table=%04X/%04X/%04X verified=%u old-rate=%02X requested=%02X rate-write=0x%08X lane-write=0x%08X restore=0x%08X fallback=0x%08X status=%s",
		count, nativeBandwidth, nativeLaneCount, rate0, rate1, rate2,
		supportedRatesMatch, oldRateSelect, requestedRateSelect,
		static_cast<uint32_t>(rateSelectWriteResult),
		static_cast<uint32_t>(laneCountWriteResult),
		static_cast<uint32_t>(rateSelectRestoreResult),
		static_cast<uint32_t>(nativeFallbackResult), status);
	return result;
}

// Replacement target for only Tahoe 25.2 LinkTraining's native DPCD 0x10A
// WriteAUX call.  V53 proved that writing 0x01 before LinkTraining is
// ineffective because this call immediately overwrites it with ASREnabled=0.
// Preserve the original ABI and forward every unmatched invocation unchanged.
// Do not add an AUX readback here: Windows performs this write immediately
// before training, and the framebuffer AUX trace records the transmitted byte.
static IOReturn writeWindowsEDPConfigurationAtNativeLinkTrainingCall(
	void *controller, void *framebuffer, uint32_t address, uint16_t length,
	void *buffer, void *displayPath) {
	if (callWriteAUX == nullptr)
		return kIOReturnNotReady;

	const auto nativeBytes = static_cast<const uint8_t *>(buffer);
	const uint8_t nativeValue = nativeBytes != nullptr && length == 1 ?
		nativeBytes[0] : static_cast<uint8_t>(0xFF);
	const bool boundedS3EDPCall =
		trainingDiagnosticsArmed != 0 &&
		panelNeedsD0Precondition != 0 &&
		targetCMLIGPUValidated != 0 &&
		isInternalEDPPath(framebuffer, displayPath) &&
		address == kDPCDEDPConfigurationSet && length == 1 &&
		nativeBytes != nullptr;
	const bool shouldSubstitute = boundedS3EDPCall && nativeValue == 0;
	uint8_t windowsValue = kWindowsEDPConfigurationSet;
	void *appliedBuffer = shouldSubstitute ?
		static_cast<void *>(&windowsValue) : buffer;
	const auto result = callWriteAUX(controller, framebuffer, address, length,
		appliedBuffer, displayPath);

	if (boundedS3EDPCall) {
		const auto count = shouldSubstitute ?
			++nativeEDPConfigurationInterceptCount :
			nativeEDPConfigurationInterceptCount;
		publishRegister32("WakeNativeEDPConfigurationInterceptCount", count);
		publishRegister32("WakeNativeEDPConfigurationBefore", nativeValue);
		publishRegister32("WakeNativeEDPConfigurationRequested",
			shouldSubstitute ? windowsValue : nativeValue);
		publishRegister32("WakeNativeEDPConfigurationWriteResult",
			static_cast<uint32_t>(result));
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"WakeNativeEDPConfigurationStatus",
				shouldSubstitute ?
					(result == kIOReturnSuccess ?
						"native-linktraining-010a-zero-replaced-with-windows-one" :
						"native-linktraining-010a-windows-one-write-failed") :
					"native-forwarded-value-was-not-exact-zero");
		SYSLOG("dpcd", "native LinkTraining eDP_CONFIGURATION_SET intercept #%u: address=0x%05X length=%u native=%02X applied=%02X substituted=%u result=0x%08X",
			count, address, length, nativeValue,
			shouldSubstitute ? windowsValue : nativeValue,
			shouldSubstitute, static_cast<uint32_t>(result));
	}
	return result;
}

static bool restoreDisplayIOBalanceLeg(void *controller) {
	if (callReadRegister32 == nullptr || callWriteRegister32 == nullptr ||
		savedDisplayIOBalanceLegValid == 0)
		return false;

	const auto before = callReadRegister32(controller, kDisplayIOBalanceLeg);
	callWriteRegister32(controller, kDisplayIOBalanceLeg,
		savedDisplayIOBalanceLeg);
	OSSynchronizeIO();
	const auto after = callReadRegister32(controller, kDisplayIOBalanceLeg);
	const bool verified = after == savedDisplayIOBalanceLeg;

	publishRegister32("Saved_BMU", savedDisplayIOBalanceLeg);
	publishRegister32("WakeBeforeRestore_BMU", before);
	publishRegister32("WakeAfterRestore_BMU", after);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("BMURestoreStatus",
			verified ? "restored-verified" : "restore-verification-failed");

	SYSLOG("power", "Gen9 DDI balance-leg restore: saved=%08X before=%08X after=%08X verified=%u",
		savedDisplayIOBalanceLeg, before, after, verified);
	return verified;
}

enum class LinuxKBLIBoostStatus : uint32_t {
	NotAttempted = 0,
	RefusedSelector,
	RefusedColdSnapshot,
	RefusedCurrentState,
	AppliedVerified,
	AppliedVerificationFailed
};

struct LinuxKBLIBoostResult {
	LinuxKBLIBoostStatus status {LinuxKBLIBoostStatus::NotAttempted};
	uint32_t attempt {0};
	uint32_t selector {0};
	uint32_t iboost {0};
	uint32_t before {0xFFFFFFFFU};
	uint32_t requested {0};
	uint32_t after {0xFFFFFFFFU};
	uint32_t verifiedCount {0};
	uint32_t refusedCount {0};
};

// Reproduce upstream i915's skl_ddi_set_iboost() for this machine's validated
// Port-A x4 eDP path.  Port A uses the A and E analog legs together.  The VBT
// has dp_iboost=0, so each KBL translation-table entry supplies its own value:
// selectors 0/1/2/4/5/7 explicitly set BALANCE_LEG_DISABLE(A/E), while
// selectors 3/6/8 program I_boost=1.  Preserve every unrelated port bit.  This
// helper deliberately performs no logging or IORegistry publication: Linux
// writes BALANCE_LEG immediately before DDI_BUF_CTL, so callers must forward
// the DDI write first and publish this result only after that critical pair.
static LinuxKBLIBoostResult applyLinuxKBLDPTrainingIBoost(void *controller,
	uint32_t ddiBufferValue) {
	LinuxKBLIBoostResult result {};
	if (callReadRegister32 == nullptr || callWriteRegister32 == nullptr ||
		trainingDiagnosticsArmed == 0 || panelNeedsD0Precondition == 0 ||
		targetCMLIGPUValidated == 0 || v61PatchFullyArmed == 0 ||
		systemTerminationPending != 0 ||
		savedDisplayIOBalanceLegValid == 0)
		return result;

	result.attempt = ++linuxKBLIBoostApplyCount;
	result.selector =
		(ddiBufferValue & kDDITrainingSelectorMask) >> 24;
	result.before = callReadRegister32(controller, kDisplayIOBalanceLeg);
	const auto savedAE =
		savedDisplayIOBalanceLeg & kBalanceLegControlledAE;
	const auto beforeAE = result.before & kBalanceLegControlledAE;
	const bool selectorValid =
		result.selector < arrsize(kLinuxKBLDPTrainingIBoost);
	const bool coldSnapshotValid =
		savedAE == kBalanceLegQuiescentAE ||
		savedAE == kBalanceLegIboost1AE ||
		savedAE == kBalanceLegIboostDisabledAE;
	const bool currentStateKnown = result.before != 0xFFFFFFFFU &&
		(beforeAE == kBalanceLegQuiescentAE ||
		 beforeAE == kBalanceLegIboost1AE ||
		 beforeAE == kBalanceLegIboostDisabledAE);

	if (!selectorValid || !coldSnapshotValid || !currentStateKnown) {
		result.refusedCount = ++linuxKBLIBoostRefusedCount;
		result.verifiedCount = linuxKBLIBoostVerifiedCount;
		result.status = !selectorValid ?
			LinuxKBLIBoostStatus::RefusedSelector :
			(!coldSnapshotValid ?
			 LinuxKBLIBoostStatus::RefusedColdSnapshot :
			 LinuxKBLIBoostStatus::RefusedCurrentState);
		return result;
	}

	result.iboost = kLinuxKBLDPTrainingIBoost[result.selector];
	const auto requestedAE = result.iboost == 0 ?
		kBalanceLegIboostDisabledAE : kBalanceLegIboost1AE;
	result.requested =
		(result.before & ~kBalanceLegControlledAE) | requestedAE;
	callWriteRegister32(controller, kDisplayIOBalanceLeg, result.requested);
	OSSynchronizeIO();
	result.after = callReadRegister32(controller, kDisplayIOBalanceLeg);
	if (result.after == result.requested) {
		result.status = LinuxKBLIBoostStatus::AppliedVerified;
		result.verifiedCount = ++linuxKBLIBoostVerifiedCount;
		result.refusedCount = linuxKBLIBoostRefusedCount;
	} else {
		result.status = LinuxKBLIBoostStatus::AppliedVerificationFailed;
		result.verifiedCount = linuxKBLIBoostVerifiedCount;
		result.refusedCount = ++linuxKBLIBoostRefusedCount;
	}
	return result;
}

static bool linuxKBLIBoostVerified(const LinuxKBLIBoostResult &result) {
	return result.status == LinuxKBLIBoostStatus::AppliedVerified;
}

static void publishLinuxKBLDPTrainingIBoost(
	const LinuxKBLIBoostResult &result, const char *stage) {
	if (result.status == LinuxKBLIBoostStatus::NotAttempted)
		return;

	const char *status = "linux-kbl-ae-iboost-write-verification-failed";
	switch (result.status) {
		case LinuxKBLIBoostStatus::RefusedSelector:
			status = "refused-selector-out-of-kbl-table";
			break;
		case LinuxKBLIBoostStatus::RefusedColdSnapshot:
			status = "refused-cold-ae-state-not-observed-working-transition-state";
			break;
		case LinuxKBLIBoostStatus::RefusedCurrentState:
			status = "refused-current-ae-state-not-known-transition-state";
			break;
		case LinuxKBLIBoostStatus::AppliedVerified:
			status = result.iboost == 0 ?
				"linux-kbl-ae-iboost-disabled-verified" :
				"linux-kbl-ae-iboost1-verified";
			break;
		case LinuxKBLIBoostStatus::AppliedVerificationFailed:
		case LinuxKBLIBoostStatus::NotAttempted:
			break;
	}

	publishRegister32("WakeLinuxKBLIBoostAttempt", result.attempt);
	publishRegister32("WakeLinuxKBLIBoostSelector", result.selector);
	publishRegister32("WakeLinuxKBLIBoostValue", result.iboost);
	publishRegister32("WakeLinuxKBLIBoostBefore", result.before);
	publishRegister32("WakeLinuxKBLIBoostRequested", result.requested);
	publishRegister32("WakeLinuxKBLIBoostAfter", result.after);
	publishRegister32("WakeLinuxKBLIBoostSavedCold",
		savedDisplayIOBalanceLeg);
	publishRegister32("WakeLinuxKBLIBoostVerifiedCount",
		result.verifiedCount);
	publishRegister32("WakeLinuxKBLIBoostRefusedCount",
		result.refusedCount);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeLinuxKBLIBoostStage", stage);
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeLinuxKBLIBoostStatus", status);
	}
	SYSLOG("training", "Linux KBL DP I_boost at %s: attempt=%u selector=%u iboost=%u before=%08X requested=%08X after=%08X status=%s verified/refused=%u/%u",
		stage, result.attempt, result.selector, result.iboost, result.before,
		result.requested, result.after, status, result.verifiedCount,
		result.refusedCount);
}

// The successful display-only wake reaches hwSetMode with DDIA already
// configured for x4 capability and four active lanes (low bits 0x16). A real
// S3 wake reaches the same point with those fields clear (0x80000000), and all
// four receiver clock-recovery bits then stay zero. DDIA/DDIE lane ownership
// must be established while the DDI buffer is disabled, before mode setting.
// Restore only those two documented fields immediately after the display
// power well comes back. DPLL0 is intentionally untouched because it also
// drives CDCLK; V33's forced disable corrupted the source clock sequence.
static bool restoreDDIAFourLaneCapability(void *controller,
	const char *stage) {
	if (callReadRegister32 == nullptr || callWriteRegister32 == nullptr)
		return false;

	const auto before = callReadRegister32(controller, kDDIBufferControlA);
	if ((before & kDDIBufferEnable) != 0) {
		publishRegister32("EarlyDDIAX4Before", before);
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("EarlyDDIAX4Status",
				"refused-ddi-a-enabled");
		SYSLOG("ddi", "%s: refused DDIA x4 restore because buffer is enabled: %08X",
			stage, before);
		return false;
	}

	const auto requested = (before & ~kDDIPortWidthMask) |
		kDDIAFourLaneCapability | kDDIPortWidthFour;
	callWriteRegister32(controller, kDDIBufferControlA, requested);
	OSSynchronizeIO();
	const auto after = callReadRegister32(controller, kDDIBufferControlA);
	const auto required = kDDIAFourLaneCapability | kDDIPortWidthFour;
	const bool verified = (after &
		(kDDIAFourLaneCapability | kDDIPortWidthMask)) == required;

	publishRegister32("EarlyDDIAX4Before", before);
	publishRegister32("EarlyDDIAX4Requested", requested);
	publishRegister32("EarlyDDIAX4After", after);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("EarlyDDIAX4Status",
			verified ? "restored-verified-before-modeset" :
			"restore-verification-failed");
	SYSLOG("ddi", "%s: early DDIA x4/four-lane restore %08X->%08X (requested %08X) verified=%u",
		stage, before, after, requested, verified);
	return verified;
}

static bool waitForRegisterMask(void *controller, uint32_t address,
	uint32_t mask, uint32_t expected) {
	if (callReadRegister32 == nullptr)
		return false;

	for (uint32_t poll = 0; poll < kPowerWellPollCount; poll++) {
		if ((callReadRegister32(controller, address) & mask) == expected)
			return true;
		IODelay(kPowerWellPollDelayUs);
	}
	return (callReadRegister32(controller, address) & mask) == expected;
}

static uint32_t readDDIAERequesterMask(void *controller) {
	if (callReadRegister32 == nullptr)
		return 0;

	uint32_t requesters = 0;
	if ((callReadRegister32(controller, kPowerWellControl1) &
		kPowerWellDDIAERequest) != 0)
		requesters |= 1U << 0;
	if ((callReadRegister32(controller, kPowerWellControl2) &
		kPowerWellDDIAERequest) != 0)
		requesters |= 1U << 1;
	if ((callReadRegister32(controller, kPowerWellControl3) &
		kPowerWellDDIAERequest) != 0)
		requesters |= 1U << 2;
	if ((callReadRegister32(controller, kPowerWellControl4) &
		kPowerWellDDIAERequest) != 0)
		requesters |= 1U << 3;
	return requesters;
}

static bool restoreDDIAERequesterTopology(void *controller,
	bool biosWasSet, bool driverWasSet) {
	if (callReadRegister32 == nullptr || callWriteRegister32 == nullptr)
		return false;

	// Keep the well powered while putting ownership back.  This avoids a
	// second unintended power edge in every failure path.
	auto driver = callReadRegister32(controller, kPowerWellControl2);
	callWriteRegister32(controller, kPowerWellControl2,
		driver | kPowerWellDDIAERequest);
	OSSynchronizeIO();
	const bool stateOn = waitForRegisterMask(controller,
		kPowerWellControl2, kPowerWellDDIAEState,
		kPowerWellDDIAEState);

	auto bios = callReadRegister32(controller, kPowerWellControl1);
	bios = biosWasSet ? bios | kPowerWellDDIAERequest :
		bios & ~kPowerWellDDIAERequest;
	callWriteRegister32(controller, kPowerWellControl1, bios);
	OSSynchronizeIO();

	if (!driverWasSet) {
		driver = callReadRegister32(controller, kPowerWellControl2);
		callWriteRegister32(controller, kPowerWellControl2,
			driver & ~kPowerWellDDIAERequest);
		OSSynchronizeIO();
	}

	const auto requesters = readDDIAERequesterMask(controller);
	const uint32_t expected = (biosWasSet ? 1U : 0U) |
		(driverWasSet ? 2U : 0U) | (requesters & 0xCU);
	return stateOn && requesters == expected;
}

static void publishCMLSourceSnapshot(void *controller,
	const char *propertyName) {
	if (callReadRegister32 == nullptr || propertyName == nullptr)
		return;

	const uint32_t values[] {
		callReadRegister32(controller, kPowerWellControl1),
		callReadRegister32(controller, kPowerWellControl2),
		callReadRegister32(controller, kPowerWellControl3),
		callReadRegister32(controller, kPowerWellControl4),
		callReadRegister32(controller, kPchResetWarningOption),
		callReadRegister32(controller, kDisplayBufferControl),
		callReadRegister32(controller, kDDIBufferControlA),
		callReadRegister32(controller, kDDIBufferControlE),
		callReadRegister32(controller, kDPTransportControlA),
		callReadRegister32(controller, kDPLLControl2),
		callReadRegister32(controller, kDPLLStatus),
		callReadRegister32(controller, kDisplayFuseStatus)
	};
	publishRegisterData(propertyName, values, arrsize(values));
}

static bool ensureGen9DisplayCorePrerequisites(void *controller) {
	if (callReadRegister32 == nullptr || callWriteRegister32 == nullptr)
		return false;

	const auto resetBefore = callReadRegister32(controller,
		kPchResetWarningOption);
	const auto resetRequested = resetBefore | kPchResetHandshakeEnable;
	if (resetRequested != resetBefore)
		callWriteRegister32(controller, kPchResetWarningOption,
			resetRequested);

	const auto powerBefore = callReadRegister32(controller,
		kPowerWellControl2);
	const auto powerRequested = powerBefore | kPowerWellPW1Request |
		kPowerWellMiscIORequest;
	if (powerRequested != powerBefore)
		callWriteRegister32(controller, kPowerWellControl2, powerRequested);

	const auto dbufBefore = callReadRegister32(controller,
		kDisplayBufferControl);
	const auto dbufRequested = dbufBefore | kDBUFPowerRequest;
	if (dbufRequested != dbufBefore)
		callWriteRegister32(controller, kDisplayBufferControl, dbufRequested);

	OSSynchronizeIO();
	const bool resetReady = waitForRegisterMask(controller,
		kPchResetWarningOption, kPchResetHandshakeEnable,
		kPchResetHandshakeEnable);
	const bool coreWellsReady = waitForRegisterMask(controller,
		kPowerWellControl2, kPowerWellPW1State | kPowerWellMiscIOState,
		kPowerWellPW1State | kPowerWellMiscIOState);
	const bool dbufReady = waitForRegisterMask(controller,
		kDisplayBufferControl, kDBUFPowerRequest | kDBUFPowerState,
		kDBUFPowerRequest | kDBUFPowerState);

	publishRegister32("CMLCoreResetBefore", resetBefore);
	publishRegister32("CMLCoreResetAfter", callReadRegister32(controller,
		kPchResetWarningOption));
	publishRegister32("CMLCorePowerWellBefore", powerBefore);
	publishRegister32("CMLCorePowerWellAfter", callReadRegister32(controller,
		kPowerWellControl2));
	publishRegister32("CMLCoreDBUFBefore", dbufBefore);
	publishRegister32("CMLCoreDBUFAfter", callReadRegister32(controller,
		kDisplayBufferControl));
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("CMLDisplayCoreStatus",
			resetReady && coreWellsReady && dbufReady ?
			"pg1-miscio-dbuf-reset-handshake-ready" :
			"prerequisite-verification-failed");
	SYSLOG("phy", "Gen9 display-core prerequisites: reset=%u PG1/MISC_IO=%u DBUF=%u PWR_CTL2=%08X DBUF_CTL=%08X",
		resetReady, coreWellsReady, dbufReady,
		callReadRegister32(controller, kPowerWellControl2),
		callReadRegister32(controller, kDisplayBufferControl));
	return resetReady && coreWellsReady && dbufReady;
}

static bool syncCMLDDIAESourcePHYOwnership(void *controller) {
	if (targetCMLIGPUValidated == 0 || cmlPhyReinitArmed == 0 ||
		cmlPhyReinitAttempted != 0 || callReadRegister32 == nullptr ||
		callWriteRegister32 == nullptr)
		return false;

	const auto ddiA = callReadRegister32(controller, kDDIBufferControlA);
	const auto ddiE = callReadRegister32(controller, kDDIBufferControlE);
	const auto transport = callReadRegister32(controller,
		kDPTransportControlA);
	publishCMLSourceSnapshot(controller, "CMLPhyBeforeSync");
	if ((ddiA & kDDIBufferEnable) != 0 ||
		(ddiA & kDDIBufferIdle) == 0 ||
		(ddiE & kDDIBufferEnable) != 0 ||
		(transport & kDPTransportEnable) != 0) {
		cmlPhyReinitAttempted = 1;
		cmlPhyReinitArmed = 0;
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("CMLPhyReinitStatus",
				"refused-ddi-or-transport-not-safely-disabled");
		SYSLOG("phy", "CML DDIA/E ownership sync refused: DDI_A=%08X DDI_E=%08X DP_TP_CTL_A=%08X",
			ddiA, ddiE, transport);
		return false;
	}

	cmlPhyReinitAttempted = 1;
	cmlPhyReinitArmed = 0;
	if (!ensureGen9DisplayCorePrerequisites(controller)) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("CMLPhyReinitStatus",
				"refused-display-core-prerequisite-failed");
		return false;
	}

	const auto biosBefore = callReadRegister32(controller,
		kPowerWellControl1);
	const auto driverBefore = callReadRegister32(controller,
		kPowerWellControl2);
	const auto kvmrBefore = callReadRegister32(controller,
		kPowerWellControl3);
	const auto debugBefore = callReadRegister32(controller,
		kPowerWellControl4);
	const auto requestersBefore = readDDIAERequesterMask(controller);
	const bool stateWasOn =
		(driverBefore & kPowerWellDDIAEState) != 0;
	publishRegister32("CMLRequesterMaskBefore", requestersBefore);
	publishRegister32("CMLPowerStateBeforeSync", stateWasOn ? 1 : 0);
	publishRegister32("CMLBIOSRequestBefore", biosBefore);
	publishRegister32("CMLDriverRequestBefore", driverBefore);
	publishRegister32("CMLKVMRRequestBefore", kvmrBefore);
	publishRegister32("CMLDebugRequestBefore", debugBefore);

	// KVMR and debug are independent owners.  Never override them.  A BIOS
	// request is different: Intel's sync path explicitly hands it to the OS by
	// first asserting the driver request and then clearing only the BIOS bit.
	if (!stateWasOn ||
		(requestersBefore & ((1U << 2) | (1U << 3))) != 0 ||
		(requestersBefore & 0x3U) == 0) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("CMLPhyReinitStatus",
			"refused-unsupported-ddi-ae-requester-topology");
		SYSLOG("phy", "CML DDIA/E ownership sync refused: state-on=%u requester mask=%u BIOS=%08X DRIVER=%08X KVMR=%08X DEBUG=%08X",
			stateWasOn, requestersBefore, biosBefore, driverBefore, kvmrBefore,
			debugBefore);
		return false;
	}

	const bool biosHandoffNeeded =
		(biosBefore & kPowerWellDDIAERequest) != 0;
	const bool driverWasSet =
		(driverBefore & kPowerWellDDIAERequest) != 0;
	if (!driverWasSet) {
		callWriteRegister32(controller, kPowerWellControl2,
			driverBefore | kPowerWellDDIAERequest);
		OSSynchronizeIO();
		if (!waitForRegisterMask(controller, kPowerWellControl2,
			kPowerWellDDIAERequest | kPowerWellDDIAEState,
			kPowerWellDDIAERequest | kPowerWellDDIAEState)) {
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty(
					"CMLPhyReinitStatus",
					"refused-driver-request-takeover-failed");
			return false;
		}
	}

	if (biosHandoffNeeded) {
		const auto biosCurrent = callReadRegister32(controller,
			kPowerWellControl1);
		callWriteRegister32(controller, kPowerWellControl1,
			biosCurrent & ~kPowerWellDDIAERequest);
		OSSynchronizeIO();
		if (!waitForRegisterMask(controller, kPowerWellControl1,
			kPowerWellDDIAERequest, 0)) {
			(void)restoreDDIAERequesterTopology(controller,
				biosHandoffNeeded, driverWasSet);
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty(
					"CMLPhyReinitStatus",
					"refused-bios-request-handoff-write-failed");
			return false;
		}
	}

	const auto biosAfterHandoff = callReadRegister32(controller,
		kPowerWellControl1);
	const auto driverAfterHandoff = callReadRegister32(controller,
		kPowerWellControl2);
	publishRegister32("CMLBIOSRequestAfterHandoff", biosAfterHandoff);
	publishRegister32("CMLDriverRequestAfterHandoff", driverAfterHandoff);
	const auto requestersAfterHandoff = readDDIAERequesterMask(controller);
	publishRegister32("CMLRequesterMaskAfterHandoff",
		requestersAfterHandoff);
	const bool stateStayedOn =
		(driverAfterHandoff &
			(kPowerWellDDIAERequest | kPowerWellDDIAEState)) ==
		(kPowerWellDDIAERequest | kPowerWellDDIAEState);
	publishRegister32("CMLPowerStateStayedOn", stateStayedOn ? 1 : 0);
	if (requestersAfterHandoff != 2U || !stateStayedOn) {
		const bool restored = restoreDDIAERequesterTopology(controller,
			biosHandoffNeeded, driverWasSet);
		publishRegister32("CMLRequesterMaskAfterUnexpectedHandoffRestore",
			readDDIAERequesterMask(controller));
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("CMLPhyReinitStatus",
				"refused-handoff-not-driver-only-or-power-state-dropped");
		SYSLOG("phy", "CML DDIA/E BIOS handoff refused: expected continuously-on driver-only requester, got mask=%u state-on=%u restore=%u",
			requestersAfterHandoff, stateStayedOn, restored);
		return false;
	}
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("CMLBIOSHandoffStatus",
			biosHandoffNeeded ?
			"bios-ddi-ae-request-transferred-to-driver" :
			"bios-ddi-ae-request-already-clear");

	const bool ddiX4 = restoreDDIAFourLaneCapability(controller,
		"post-CML-DDIAE-ownership-sync");
	const auto count = ++cmlPhyReinitCount;
	publishRegister32("CMLPhyReinitCount", count);
	publishRegister32("CMLRequesterMaskAfterReinit",
		readDDIAERequesterMask(controller));
	publishCMLSourceSnapshot(controller, "CMLPhyAfterSync");
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("CMLPhyReinitStatus",
			ddiX4 ?
			"bios-handoff-without-power-cycle-completed-x4-restored" :
			"bios-handoff-without-power-cycle-x4-restore-failed");
	SYSLOG("phy", "CML 9BC4 DDIA/E ownership sync #%u: BIOS-handoff=%u continuous-power=%u x4=%u requesters=%u PWR_CTL2=%08X DDI_A=%08X",
		count, biosHandoffNeeded, stateStayedOn, ddiX4,
		readDDIAERequesterMask(controller),
		callReadRegister32(controller, kPowerWellControl2),
		callReadRegister32(controller, kDDIBufferControlA));
	return ddiX4;
}

static uint32_t calculateAppleRawClock(uint32_t current,
	uint32_t southFuse) {
	return (current & kAppleRawClockPreserveMask) |
		((southFuse & kSouthFuseRawFrequency24MHz) != 0 ?
			kAppleRawClock24MHz : kAppleRawClock19Point2MHz);
}

static uint32_t calculateWindowsRawClock(uint32_t current,
	uint32_t southFuse) {
	return (current & ((southFuse & kSouthFuseRawFrequency24MHz) != 0 ?
		kWindowsRawClock24MHzPreserveMask :
		kWindowsRawClock19Point2MHzPreserveMask)) |
		((southFuse & kSouthFuseRawFrequency24MHz) != 0 ?
			kWindowsRawClock24MHz : kWindowsRawClock19Point2MHz);
}

static bool captureColdPchRawClock(void *controller, const char *stage) {
	if (targetCMLIGPUValidated == 0 || controller == nullptr ||
		callReadRegister32 == nullptr)
		return false;
	if (savedPchRawClockFrequencyValid != 0)
		return true;

	const auto rawClock = callReadRegister32(controller,
		kPchRawClockFrequency);
	const auto southFuse = callReadRegister32(controller, kSouthFuseStrap);
	if (rawClock == 0xFFFFFFFFU || southFuse == 0xFFFFFFFFU) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"ColdRawClockCaptureStatus", "refused-invalid-mmio-read");
		return false;
	}

	savedPchRawClockFrequency = rawClock;
	savedSouthFuseStrap = southFuse;
	OSSynchronizeIO();
	savedPchRawClockFrequencyValid = 1;
	publishRegister32("ColdRawClockFrequency", rawClock);
	publishRegister32("ColdRawClockSouthFuse", southFuse);
	publishRegister32("ColdRawClockAppleFormula",
		calculateAppleRawClock(rawClock, southFuse));
	publishRegister32("ColdRawClockWindowsD0Formula",
		calculateWindowsRawClock(rawClock, southFuse));
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty("ColdRawClockCaptureStage",
			stage != nullptr ? stage : "unspecified");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"ColdRawClockCaptureStatus", "captured-known-working-value");
	}
	SYSLOG("rawclk", "captured cold working PCH RAWCLK: value=%08X SFUSE=%08X Apple-formula=%08X Windows-D0-formula=%08X stage=%s",
		rawClock, southFuse, calculateAppleRawClock(rawClock, southFuse),
		calculateWindowsRawClock(rawClock, southFuse),
		stage != nullptr ? stage : "unspecified");
	return true;
}

static bool restoreColdPchRawClock(void *controller, const char *stage) {
	if (targetCMLIGPUValidated == 0 || controller == nullptr ||
		callReadRegister32 == nullptr || callWriteRegister32 == nullptr)
		return false;
	if (wakeRawClockRestoreAttempted != 0) {
		const auto current = callReadRegister32(controller,
			kPchRawClockFrequency);
		return savedPchRawClockFrequencyValid != 0 &&
			current == savedPchRawClockFrequency;
	}
	wakeRawClockRestoreAttempted = 1;
	publishRegister32("WakeRawClockRestoreAttempted", 1);
	if (savedPchRawClockFrequencyValid == 0) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"WakeRawClockRestoreStatus",
				"refused-no-known-working-cold-value");
		SYSLOG("rawclk", "S3 RAWCLK restore refused: no known-working cold value was captured");
		return false;
	}

	const auto before = callReadRegister32(controller, kPchRawClockFrequency);
	const auto southFuse = callReadRegister32(controller, kSouthFuseStrap);
	const auto requested = savedPchRawClockFrequency;
	publishRegister32("WakeRawClockBefore", before);
	publishRegister32("WakeRawClockSouthFuse", southFuse);
	publishRegister32("WakeRawClockSavedCold", requested);
	publishRegister32("WakeRawClockAppleFormulaFromBefore",
		calculateAppleRawClock(before, southFuse));
	publishRegister32("WakeRawClockWindowsD0FormulaFromBefore",
		calculateWindowsRawClock(before, southFuse));
	publishRegister32("WakeRawClockSouthFuseMatchesCold",
		southFuse == savedSouthFuseStrap ? 1 : 0);

	// Windows re-latches this register on every supported D0 transition even
	// when its calculated value already matches.  Do the same, but with the
	// exact value that made this panel work during the current macOS boot.
	callWriteRegister32(controller, kPchRawClockFrequency, requested);
	OSSynchronizeIO();
	IODelay(kRawClockRelatchDelayUs);
	const auto after = callReadRegister32(controller, kPchRawClockFrequency);
	const bool verified = after == requested;
	publishRegister32("WakeRawClockRequested", requested);
	publishRegister32("WakeRawClockAfter", after);
	publishRegister32("WakeRawClockVerified", verified ? 1 : 0);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty("WakeRawClockRestoreStage",
			stage != nullptr ? stage : "unspecified");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeRawClockRestoreStatus", verified ?
				"cold-working-value-relatched-and-verified" :
				"cold-working-value-write-did-not-stick");
	}
	SYSLOG("rawclk", "S3 PCH RAWCLK re-latch: before=%08X saved=%08X requested=%08X after=%08X SFUSE=%08X verified=%u stage=%s",
		before, savedPchRawClockFrequency, requested, after, southFuse,
		verified, stage != nullptr ? stage : "unspecified");
	return verified;
}

static bool captureColdSouthChicken1(const char *stage) {
	if (targetCMLIGPUValidated == 0 || igpuBAR0MappingValid == 0 ||
		southChickenLock == nullptr)
		return false;

	IOLockLock(southChickenLock);
	if (savedSouthChicken1Valid != 0) {
		IOLockUnlock(southChickenLock);
		return true;
	}
	const auto value = readIGPUDisplayMMIO32(kSouthChicken1);
	if (value != 0xFFFFFFFFU) {
		savedSouthChicken1 = value;
		OSSynchronizeIO();
		savedSouthChicken1Valid = 1;
	}
	IOLockUnlock(southChickenLock);
	if (value == 0xFFFFFFFFU) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"ColdSouthChickenCaptureStatus", "refused-invalid-mmio-read");
		return false;
	}
	publishRegister32("ColdSouthChicken1", value);
	publishRegister32("ColdSouthChickenSBCLKRunRefclkDisable",
		(value & kSBCLKRunRefclkDisable) != 0 ? 1 : 0);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty(
			"ColdSouthChickenCaptureStage",
			stage != nullptr ? stage : "unspecified");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"ColdSouthChickenCaptureStatus",
			(value & kSBCLKRunRefclkDisable) == 0 ?
			"captured-working-resume-bit-clear" :
			"captured-but-working-resume-bit-unexpectedly-set");
	}
	SYSLOG("refclk", "captured cold working SOUTH_CHICKEN1=%08X bit7=%u stage=%s",
		value, (value & kSBCLKRunRefclkDisable) != 0,
		stage != nullptr ? stage : "unspecified");
	return true;
}

static bool setSouthChickenReferenceClockForSleep(const char *stage) {
	if (southChickenLock == nullptr)
		return false;

	uint32_t attempt = 0;
	uint32_t before = 0xFFFFFFFFU;
	uint32_t requested = 0xFFFFFFFFU;
	uint32_t after = 0xFFFFFFFFU;
	bool writeIssued = false;
	bool verified = false;
	const char *refusal = nullptr;
	IOLockLock(southChickenLock);
	if (targetCMLIGPUValidated == 0 || v61PatchFullyArmed == 0 ||
		igpuBAR0MappingValid == 0 || savedSouthChicken1Valid == 0 ||
		(savedSouthChicken1 & kSBCLKRunRefclkDisable) != 0) {
		refusal = "refused-no-validated-working-bit-clear-baseline";
	} else if (systemSleepPending == 0 ||
		southChickenSuspendSetArmed == 0 ||
		southChickenPanelOffCompleted == 0) {
		refusal = "refused-not-committed-sleep-after-verified-panel-off";
	} else if (southChickenSleepSetAttempted >=
		kSouthChickenSleepSetMaxAttempts) {
		refusal = "refused-bounded-device-off-set-attempt-consumed";
	} else {
		attempt = ++southChickenSleepSetAttempted;
		// Claim the one exact device-off edge before touching MMIO.  A duplicate
		// notification must never repeat the boundary write in this generation.
		southChickenSuspendSetArmed = 0;
		before = readIGPUDisplayMMIO32(kSouthChicken1);
		if (before == 0xFFFFFFFFU) {
			refusal = "refused-invalid-mmio-read";
		} else {
			requested = before | kSBCLKRunRefclkDisable;
			// intel_uncore_rmw() issues the write even when the old value already
			// has the requested bit.  Preserve that edge/relatch behaviour.
			// Arm cleanup before issuing it: if readback becomes unavailable at the
			// D3 boundary, the write may still have reached hardware.
			southChickenSleepSetWriteIssued = 1;
			southChickenSleepSetVerified = 0;
			southChickenWakeClearAttempted = 0;
			southChickenWakeClearVerified = 0;
			southChickenWakeClearArmed = 1;
			writeIssued = true;
			writeIGPUDisplayMMIO32(kSouthChicken1, requested);
			OSSynchronizeIO();
			after = readIGPUDisplayMMIO32(kSouthChicken1);
			verified = after != 0xFFFFFFFFU &&
				(after & kSBCLKRunRefclkDisable) != 0;
			if (verified) {
				southChickenSleepSetVerified = 1;
			}
		}
	}
	IOLockUnlock(southChickenLock);

	if (refusal != nullptr) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"SleepSouthChickenSetStatus", refusal);
		return false;
	}
	publishRegister32("SleepSouthChickenSetAttempt", attempt);
	publishRegister32("SleepSouthChickenBefore", before);
	publishRegister32("SleepSouthChickenRequested", requested);
	publishRegister32("SleepSouthChickenAfter", after);
	publishRegister32("SleepSouthChickenSetWriteIssued", writeIssued ? 1 : 0);
	publishRegister32("SleepSouthChickenSetVerified", verified ? 1 : 0);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty(
			"SleepSouthChickenSetStage",
			stage != nullptr ? stage : "unspecified");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"SleepSouthChickenSetStatus", verified ?
			"linux-cnp-suspend-bit-set-and-readback-verified" :
			(writeIssued ?
			"suspend-bit-write-issued-readback-unverified-clear-remains-armed" :
			"suspend-bit-write-not-issued"));
	}
	SYSLOG("refclk", "CNP IGPU-device-down SOUTH_CHICKEN1 bit7 set #%u: before=%08X requested=%08X after=%08X write-issued=%u verified=%u wake-clear-armed=%u stage=%s",
		attempt, before, requested, after, writeIssued, verified,
		southChickenWakeClearArmed,
		stage != nullptr ? stage : "unspecified");
	return verified;
}

static bool restoreSouthChickenReferenceClockBeforeResume(const char *stage) {
	if (southChickenLock == nullptr)
		return false;

	uint32_t attempt = 0;
	uint32_t before = 0xFFFFFFFFU;
	uint32_t requested = 0xFFFFFFFFU;
	uint32_t after = 0xFFFFFFFFU;
	bool verified = false;
	const char *refusal = nullptr;
	IOLockLock(southChickenLock);
	if (southChickenWakeClearArmed == 0) {
		refusal = "not-armed";
	} else if (targetCMLIGPUValidated == 0 || v61PatchFullyArmed == 0 ||
		igpuBAR0MappingValid == 0) {
		refusal = "refused-unvalidated-or-unmapped-igpu";
	} else if (southChickenWakeClearAttempted >=
		kSouthChickenWakeClearMaxAttempts) {
		refusal = "refused-bounded-clear-attempts-exhausted";
	} else if (southChickenSleepSetWriteIssued == 0 ||
		savedSouthChicken1Valid == 0 ||
		(savedSouthChicken1 & kSBCLKRunRefclkDisable) != 0) {
		refusal = "refused-no-validated-working-bit-clear-baseline";
	} else {
		attempt = ++southChickenWakeClearAttempted;
		before = readIGPUDisplayMMIO32(kSouthChicken1);
		if (before == 0xFFFFFFFFU) {
			refusal = "refused-invalid-mmio-read";
		} else {
			requested = before & ~kSBCLKRunRefclkDisable;
			// Linux issues an unconditional RMW even when bit7 already reads clear.
			// Our readback additionally flushes and proves this pre-FB edge.
			writeIGPUDisplayMMIO32(kSouthChicken1, requested);
			OSSynchronizeIO();
			after = readIGPUDisplayMMIO32(kSouthChicken1);
			verified = after != 0xFFFFFFFFU &&
				(after & kSBCLKRunRefclkDisable) == 0;
			if (verified) {
				southChickenWakeClearArmed = 0;
				southChickenWakeClearVerified = 1;
				southChickenSleepSetWriteIssued = 0;
				southChickenSleepSetVerified = 0;
			}
		}
	}
	IOLockUnlock(southChickenLock);

	if (refusal != nullptr) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"WakeSouthChickenClearStatus", refusal);
		return false;
	}
	publishRegister32("WakeSouthChickenClearAttempt", attempt);
	publishRegister32("WakeSouthChickenBefore", before);
	publishRegister32("WakeSouthChickenRequested", requested);
	publishRegister32("WakeSouthChickenAfter", after);
	publishRegister32("WakeSouthChickenClearVerified", verified ? 1 : 0);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeSouthChickenClearStage",
			stage != nullptr ? stage : "unspecified");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeSouthChickenClearStatus", verified ?
			(requested == before ?
			"linux-cnp-resume-clear-relatched-and-readback-verified" :
			"linux-cnp-resume-bit-cleared-and-readback-verified") :
			"resume-bit-clear-write-did-not-stick");
	}
	SYSLOG("refclk", "CNP pre-FB-resume SOUTH_CHICKEN1 bit7 clear #%u: before=%08X requested=%08X after=%08X verified=%u stage=%s",
		attempt, before, requested, after, verified,
		stage != nullptr ? stage : "unspecified");
	return verified;
}

// Restore CNP Display WA #1179 at the matching HPD boundary, independently
// from the earlier resume-early bit7 clear.  V56 proves that this machine is
// cold-working at duration F but reaches this exact point at duration 9.  The
// intervention is deliberately one-shot and refuses every other live value;
// it never writes the whole register and never retries inside a generation.
static bool restoreSouthChickenHotPlugDurationBeforePanelOn(void *controller,
	const char *stage) {
	if (controller == nullptr || callReadRegister32 == nullptr ||
		callWriteRegister32 == nullptr || southChickenLock == nullptr)
		return false;

	uint32_t attempt = 0;
	uint32_t before = 0xFFFFFFFFU;
	uint32_t requested = 0xFFFFFFFFU;
	uint32_t after = 0xFFFFFFFFU;
	bool writeIssued = false;
	bool verified = false;
	const char *status = nullptr;

	IOLockLock(southChickenLock);
	if (targetCMLIGPUValidated == 0 || v61PatchFullyArmed == 0 ||
		igpuBAR0MappingValid == 0 || savedSouthChicken1Valid == 0) {
		status = "refused-unvalidated-unmapped-or-no-cold-baseline";
	} else if ((savedSouthChicken1 & kChassisClockRequestDurationMask) !=
		kChassisClockRequestDurationColdExpected ||
		(savedSouthChicken1 & kSBCLKRunRefclkDisable) != 0) {
		status = "refused-cold-baseline-not-duration-f-bit7-clear";
	} else if (southChickenDurationRestoreArmed == 0 ||
		southChickenDurationRestoreAttempted != 0) {
		status = "refused-not-armed-or-one-shot-consumed";
	} else if (systemSleepPending != 0 ||
		southChickenResumeBoundaryObserved == 0 ||
		southChickenWakeClearVerified == 0 ||
		southChickenPanelOffCompleted == 0 ||
		panelNeedsD0Precondition == 0 || cmlPhyReinitArmed == 0) {
		status = "refused-not-first-committed-s3-panel-on-after-bit7-clear";
	} else {
		attempt = ++southChickenDurationRestoreAttempted;
		southChickenDurationRestoreArmed = 0;
		before = callReadRegister32(controller, kSouthChicken1);
		if (before == 0xFFFFFFFFU) {
			status = "refused-invalid-pre-hpd-mmio-read";
		} else if ((before & kSBCLKRunRefclkDisable) != 0) {
			status = "refused-pre-hpd-bit7-still-set";
		} else if ((before & kChassisClockRequestDurationMask) ==
			kChassisClockRequestDurationColdExpected) {
			status = "not-needed-pre-hpd-duration-already-cold-no-write";
		} else if ((before & kChassisClockRequestDurationMask) !=
			kChassisClockRequestDurationWakeExpected) {
			status = "refused-unexpected-pre-hpd-duration-not-nine";
		} else {
			requested =
				(before & ~kChassisClockRequestDurationMask) |
				(savedSouthChicken1 &
					kChassisClockRequestDurationMask);
			southChickenDurationRestoreWriteIssued = 1;
			writeIssued = true;
			callWriteRegister32(controller, kSouthChicken1, requested);
			OSSynchronizeIO();
			after = callReadRegister32(controller, kSouthChicken1);
			verified = after != 0xFFFFFFFFU &&
				(after & kChassisClockRequestDurationMask) ==
					(savedSouthChicken1 &
						kChassisClockRequestDurationMask) &&
				(after & ~kChassisClockRequestDurationMask) ==
					(before & ~kChassisClockRequestDurationMask);
			if (verified)
				southChickenDurationRestoreVerified = 1;
			status = verified ?
				"cnp-wa1179-duration-f-restored-pre-hpd-and-verified" :
				"cnp-wa1179-duration-write-issued-readback-not-exact";
		}
	}
	IOLockUnlock(southChickenLock);

	publishRegister32("WakeSouthChickenDurationRestoreAttempt", attempt);
	publishRegister32("WakeSouthChickenDurationBeforeHPD", before);
	publishRegister32("WakeSouthChickenDurationRequested", requested);
	publishRegister32("WakeSouthChickenDurationAfterWrite", after);
	publishRegister32("WakeSouthChickenDurationWriteIssued",
		writeIssued ? 1 : 0);
	publishRegister32("WakeSouthChickenDurationRestoreVerified",
		verified ? 1 : 0);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeSouthChickenDurationRestoreStage",
			stage != nullptr ? stage : "unspecified");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeSouthChickenDurationRestoreStatus",
			status != nullptr ? status : "unknown");
	}
	SYSLOG("refclk", "CNP WA1179 pre-HPD SOUTH_CHICKEN1 duration restore #%u: before=%08X cold=%08X requested=%08X after=%08X write-issued=%u verified=%u stage=%s status=%s",
		attempt, before, savedSouthChicken1, requested, after, writeIssued,
		verified, stage != nullptr ? stage : "unspecified",
		status != nullptr ? status : "unknown");
	return verified;
}

static IOReturn igpuPowerInterest(void *, void *, UInt32 messageType,
	IOService *provider, void *messageArgument, vm_size_t argumentSize) {
	if (provider == nullptr || provider != igpuPowerService) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"IGPUPowerBoundaryStatus", "refused-unexpected-provider");
		return kIOReturnSuccess;
	}

	const auto notification = messageArgument != nullptr &&
		argumentSize >= sizeof(IOPowerStateChangeNotification) ?
		static_cast<IOPowerStateChangeNotification *>(messageArgument) : nullptr;
	switch (messageType) {
		case kIOMessageDeviceWillPowerOff: {
			const bool exactOff = notification != nullptr &&
				notification->stateNumber == kIOPCIDeviceOffState;
			const bool setResult = exactOff &&
				setSouthChickenReferenceClockForSleep(
					"igpu-general-interest-will-power-off-state0");
			const auto count = ++igpuDeviceWillPowerOffCount;
			publishRegister32("IGPUDeviceWillPowerOffCount", count);
			publishRegister32("IGPUDeviceWillPowerOffTargetState",
				notification != nullptr ?
					static_cast<uint32_t>(notification->stateNumber) : 0xFFFFFFFFU);
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty(
					"IGPUPowerBoundaryLastEvent", exactOff ?
					(setResult ? "will-off-state0-refclk-set-verified" :
					"will-off-state0-refclk-set-skipped-or-failed") :
					"will-off-nonzero-or-missing-state-ignored");
			break;
		}
		case kIOMessageDeviceWillNotPowerOff: {
			// A cancelled device transition leaves BAR0 accessible.  Undo this
			// generation's late set immediately; never carry bit7 into normal use.
			const bool clearWasArmed = southChickenWakeClearArmed != 0;
			bool clearResult = !clearWasArmed;
			for (uint32_t retry = 0;
				clearWasArmed && !clearResult &&
				retry < kSouthChickenWakeClearMaxAttempts; retry++) {
				clearResult = restoreSouthChickenReferenceClockBeforeResume(
					"igpu-general-interest-will-not-power-off-cancel");
				if (!clearResult)
					IODelay(10);
			}
			// A WillNotPowerOff for an earlier, nonzero transition must not consume
			// the still-pending final state-0 edge.  Only a generation for which our
			// exact WillPowerOff(state0) write was issued owns cleanup/disarm here.
			if (clearWasArmed) {
				if (southChickenLock != nullptr)
					IOLockLock(southChickenLock);
				southChickenSuspendSetArmed = 0;
				OSSynchronizeIO();
				if (southChickenLock != nullptr)
					IOLockUnlock(southChickenLock);
			}
			publishRegister32("IGPUDeviceWillNotPowerOffCount",
				++igpuDeviceWillNotPowerOffCount);
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty(
					"IGPUPowerBoundaryLastEvent", !clearWasArmed ?
					"will-not-off-no-refclk-set-observed" :
					(clearResult ? "will-not-off-refclk-clear-verified" :
					"will-not-off-refclk-clear-failed-remains-armed"));
			break;
		}
		case kIOMessageDeviceHasPoweredOff:
			publishRegister32("IGPUDeviceHasPoweredOffCount",
				++igpuDeviceHasPoweredOffCount);
			break;
		case kIOMessageDeviceWillPowerOn:
			publishRegister32("IGPUDeviceWillPowerOnCount",
				++igpuDeviceWillPowerOnCount);
			break;
		case kIOMessageDeviceHasPoweredOn:
			publishRegister32("IGPUDeviceHasPoweredOnCount",
				++igpuDeviceHasPoweredOnCount);
			// General-interest HasPoweredOn may be later than framebuffer child
			// recovery.  It is only a bounded cleanup fallback; FB0's routed
			// Sleep->Wake callback is the primary early-clear boundary.
			southChickenResumeBoundaryObserved = 1;
			OSSynchronizeIO();
			if (southChickenWakeClearArmed != 0)
				(void)restoreSouthChickenReferenceClockBeforeResume(
					"igpu-general-has-powered-on-late-cleanup-fallback");
			break;
		default:
			break;
	}
	return kIOReturnSuccess;
}

static bool initIGPUPowerBoundary(IOPCIDevice *igpu) {
	if (igpu == nullptr)
		return false;
	if (igpuPowerNotifier != nullptr && igpuBAR0MappingValid != 0 &&
		igpuPowerService == igpu)
		return true;

	if (southChickenLock == nullptr)
		southChickenLock = IOLockAlloc();
	if (southChickenLock == nullptr) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"IGPUPowerBoundaryStatus", "refused-lock-allocation-failed");
		return false;
	}

	auto map = igpu->mapDeviceMemoryWithIndex(0, kIOMapInhibitCache);
	if (map == nullptr || map->getAddress() == 0 ||
		map->getLength() < kSouthChicken1 + sizeof(uint32_t)) {
		if (map != nullptr)
			map->release();
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"IGPUPowerBoundaryStatus", "refused-bar0-map-or-length-invalid");
		return false;
	}

	IOLockLock(southChickenLock);
	igpuBAR0Map = map;
	igpuBAR0Address = reinterpret_cast<volatile uint8_t *>(map->getAddress());
	igpuBAR0Length = map->getLength();
	OSSynchronizeIO();
	igpuBAR0MappingValid = 1;
	IOLockUnlock(southChickenLock);
	if (!captureColdSouthChicken1("validated-igpu-bar0-cold-working" ) ||
		(savedSouthChicken1 & kSBCLKRunRefclkDisable) != 0) {
		IOLockLock(southChickenLock);
		igpuBAR0MappingValid = 0;
		igpuBAR0Address = nullptr;
		igpuBAR0Length = 0;
		igpuBAR0Map = nullptr;
		IOLockUnlock(southChickenLock);
		map->release();
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"IGPUPowerBoundaryStatus",
				"refused-no-cold-working-c2000-bit7-clear-baseline");
		return false;
	}

	igpuPowerService = igpu;
	igpuPowerService->retain();
	igpuPowerNotifier = igpu->registerInterest(gIOGeneralInterest,
		igpuPowerInterest, nullptr, nullptr);
	if (igpuPowerNotifier == nullptr) {
		igpuPowerService->release();
		igpuPowerService = nullptr;
		IOLockLock(southChickenLock);
		igpuBAR0MappingValid = 0;
		igpuBAR0Address = nullptr;
		igpuBAR0Length = 0;
		igpuBAR0Map = nullptr;
		IOLockUnlock(southChickenLock);
		map->release();
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"IGPUPowerBoundaryStatus", "refused-general-interest-registration-failed");
		return false;
	}

	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty("IGPUBAR0Length",
			static_cast<uint64_t>(igpuBAR0Length), 64);
		RazerOLEDWakeFix_selfInstance->setProperty("IGPUPowerBoundaryStatus",
			"bar0-mapped-cold-baseline-validated-general-interest-registered");
		RazerOLEDWakeFix_selfInstance->setProperty("IGPUResumeClearBoundary",
			"FB0-doSetPowerState-Sleep-to-Wake-pre-original");
	}
	SYSLOG("refclk", "validated IGPU BAR0 map length=0x%llX and registered general-interest device power boundary; WillOff(state0) sets C2000 bit7, FB0 Sleep->Wake pre-original clears it",
		static_cast<unsigned long long>(igpuBAR0Length));
	return true;
}

static uint32_t wrapFramebufferDoSetPowerState(void *framebuffer,
	uint32_t newState) {
	const auto oldState = framebuffer != nullptr ?
		*reinterpret_cast<const uint32_t *>(
			static_cast<const uint8_t *>(framebuffer) +
			kFramebufferPowerStateOffset) : 0xFFFFFFFFU;
	const auto effectiveNewState = newState < kFramebufferWakeState ?
		newState : kFramebufferWakeState;
	const bool fb0SleepToWake = isInternalFramebuffer(framebuffer) &&
		oldState == kFramebufferSleepState &&
		effectiveNewState == kFramebufferWakeState;
	if (fb0SleepToWake) {
		publishRegister32("FramebufferSleepToWakeCount",
			++framebufferSleepToWakeCount);
		publishRegister32("FramebufferWakeRequestedState", newState);
		southChickenResumeBoundaryObserved = 1;
		OSSynchronizeIO();
		const bool clearWasArmed = southChickenWakeClearArmed != 0;
		const bool clearResult = clearWasArmed &&
			restoreSouthChickenReferenceClockBeforeResume(
				"fb0-doSetPowerState-sleep-to-wake-before-original");
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"FramebufferWakeRefclkClearStatus", clearWasArmed ?
				(clearResult ? "pre-original-clear-verified" :
				"pre-original-clear-failed-fallback-remains-armed") :
				"not-needed-no-device-off-set-observed");
		SYSLOG("refclk", "FB0 doSetPowerState Sleep->Wake pre-original boundary: requested=%u clear-armed=%u clear-verified=%u",
			newState, clearWasArmed, clearResult);
	}
	return originalFramebufferDoSetPowerState(framebuffer, newState);
}

static void wrapSetPowerWellState(void *controller, void *framebuffer,
	bool enable) {
	const bool internalFramebuffer = isInternalFramebuffer(framebuffer);
	if (enable && southChickenWakeClearArmed != 0 &&
		(southChickenResumeBoundaryObserved != 0 ||
		systemSleepPending == 0))
		(void)restoreSouthChickenReferenceClockBeforeResume(
			framebuffer == nullptr ?
			"before-native-global-power-well-enable-bounded-retry" :
			(internalFramebuffer ?
			"before-native-fb0-power-well-enable-retry" :
			"before-native-external-power-well-enable-retry"));

	originalSetPowerWellState(controller, framebuffer, enable);
	if (!enable)
		return;
	if (panelNeedsD0Precondition != 0) {
		(void)restoreColdPchRawClock(controller,
			"post-native-power-well-enable");
		(void)restoreDDIAFourLaneCapability(controller,
			"post-power-well-enable");
	}
}

// Apple builds a 0x370-byte CRTCParams block before touching the hardware.
// On a working cold/display-only modeset, offset 0x44 is 0x88000016 for this
// panel: translation selector 8, DDIA x4 capability, and four-lane width.  A
// true S3 wake rebuilds the same field as 0x80000000.  Restoring the MMIO
// register later is insufficient because the already-corrupted CRTCParams is
// what LightUpEDP eventually programs.  Capture the exact working value and,
// only for FB0/eDP/DDIA after an armed panel-off, restore its documented DDI
// selector/lane fields immediately after Apple's native SetupParams returns.
static bool isInternalFramebuffer(void *framebuffer) {
	if (framebuffer == nullptr)
		return false;

	const auto framebufferBytes = static_cast<const uint8_t *>(framebuffer);
	const auto framebufferIndex = *reinterpret_cast<const uint32_t *>(
		framebufferBytes + kFramebufferIndexOffset);
	return framebufferIndex == kInternalFramebufferIndex;
}

static bool isInternalEDPPath(void *framebuffer, void *displayPath) {
	if (!isInternalFramebuffer(framebuffer) || displayPath == nullptr)
		return false;

	const auto pathBytes = static_cast<const uint8_t *>(displayPath);
	const auto portType = *reinterpret_cast<const uint32_t *>(
		pathBytes + kDisplayPathTypeOffset);
	const auto portIndex = *(pathBytes + kDisplayPathPortOffset);
	return portType == kEDPPortType && portIndex == kDDIAPortIndex;
}

static void wrapSetupParams(void *controller, void *framebuffer,
	void *displayPath, void *crtcParams, const void *timing) {
	originalSetupParams(controller, framebuffer, displayPath, crtcParams,
		timing);
	if (crtcParams == nullptr || !isInternalEDPPath(framebuffer, displayPath))
		return;

	auto target = reinterpret_cast<volatile uint32_t *>(
		static_cast<uint8_t *>(crtcParams) +
		kCRTCParamsDDIBufferControlOffset);
	const auto before = *target;
	const auto required = kDDIAFourLaneCapability | kDDIPortWidthFour;

	if (panelNeedsD0Precondition == 0) {
		if ((before & kDDIBufferEnable) != 0 &&
			(before & (kDDIAFourLaneCapability | kDDIPortWidthMask)) == required) {
			savedSetupDDIBufferControl = before;
			savedSetupDDIBufferControlValid = 1;
			publishRegister32("ColdSetupDDIBufferControl", before);
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty(
					"SetupDDICaptureStatus", "captured-working-fb0-edp-ddia");
		}
		return;
	}

	publishRegister32("WakeSetupDDIBefore", before);
	if (savedSetupDDIBufferControlValid == 0) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"WakeSetupDDIStatus", "refused-no-cold-working-params");
		SYSLOG("ddi", "SetupParams restore refused: no cold working FB0/eDP/DDIA value captured");
		return;
	}

	const auto requested = (before & ~kDDISetupWorkingFieldMask) |
		(savedSetupDDIBufferControl & kDDISetupWorkingFieldMask);
	*target = requested;
	const auto after = *target;
	const bool verified = after == requested &&
		(after & (kDDIAFourLaneCapability | kDDIPortWidthMask)) == required;
	publishRegister32("WakeSetupDDISaved", savedSetupDDIBufferControl);
	publishRegister32("WakeSetupDDIRequested", requested);
	publishRegister32("WakeSetupDDIAfter", after);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("WakeSetupDDIStatus",
			verified ? "restored-working-fields-before-hardware-modeset" :
			"restore-verification-failed");
	SYSLOG("ddi", "SetupParams FB0/eDP/DDIA restore: saved=%08X before=%08X requested=%08X after=%08X verified=%u",
		savedSetupDDIBufferControl, before, requested, after, verified);
}

static bool restoreRazerPanelPowerSequence(void *controller) {
	if (callReadRegister32 == nullptr || callWriteRegister32 == nullptr)
		return false;

	const auto oldOn = callReadRegister32(controller, kPanelPowerOnDelays);
	const auto oldOff = callReadRegister32(controller, kPanelPowerOffDelays);
	const auto oldDivisor = callReadRegister32(controller, kPanelPowerDivisor);

	callWriteRegister32(controller, kPanelPowerOnDelays,
		kRazerPanelPowerOnDelays);
	callWriteRegister32(controller, kPanelPowerOffDelays,
		kRazerPanelPowerOffDelays);
	OSSynchronizeIO();

	const auto newOn = callReadRegister32(controller, kPanelPowerOnDelays);
	const auto newOff = callReadRegister32(controller, kPanelPowerOffDelays);
	const auto newDivisor = callReadRegister32(controller, kPanelPowerDivisor);
	const bool verified = newOn == kRazerPanelPowerOnDelays &&
		newOff == kRazerPanelPowerOffDelays;

	// On this Coffee Lake controller 0xC7210 reads as 0xFFFFFFFF in both the
	// working cold state and S3 wake state.  It is not a writable Gen9 PPS
	// prerequisite, so do not make successful timing restoration depend on it.
	SYSLOG("power", "Razer BIOS VBT PPS timing restore: before=%08X/%08X divisor=%08X after=%08X/%08X divisor=%08X verified=%u",
		oldOn, oldOff, oldDivisor, newOn, newOff, newDivisor, verified);
	return verified;
}

static void wrapHwSetPanelPowerConfig(void *controller, uint32_t value) {
	originalHwSetPanelPowerConfig(controller, value);
	if (systemTerminationPending != 0)
		return;
	if (forceRazerPanelPowerConfig == 0 || controller == nullptr)
		return;

	// Tahoe reports "Using the default EDP panel timings" on this non-Apple
	// Samsung OLED.  hwSetPanelPower copies these two cached fields into the
	// MMIO PPS registers after hwSetPanelPowerConfig returns, which previously
	// overwrote our board-specific values.  Replace exactly the two fields the
	// native routine copies; leave its control/state-machine fields untouched.
	auto controllerBytes = static_cast<uint8_t *>(controller);
	auto cachedOn = reinterpret_cast<volatile uint32_t *>(controllerBytes +
		kControllerPanelPowerOnDelaysOffset);
	auto cachedOff = reinterpret_cast<volatile uint32_t *>(controllerBytes +
		kControllerPanelPowerOffDelaysOffset);
	*cachedOn = kRazerPanelPowerOnDelays;
	*cachedOff = kRazerPanelPowerOffDelays;
	OSSynchronizeIO();
	publishRegister32("WakePanelCached_PP_ON_DELAYS", *cachedOn);
	publishRegister32("WakePanelCached_PP_OFF_DELAYS", *cachedOff);
	SYSLOG("power", "overrode Tahoe default cached eDP timings for native OLED receiver cycle: ON=%08X OFF=%08X",
		*cachedOn, *cachedOff);
}

static uint32_t wrapHwSetPanelPower(void *controller, uint32_t state) {
	if (state == 0 && systemTerminationPending != 0) {
		// Priority root-domain notifications are the only documented way for a
		// kext to distinguish shutdown/restart from ordinary display blanking.
		// Never capture, arm, delay, log, publish or suppress this physical
		// panel-off: forward it as the first operation in the wrapper.
		panelNeedsD0Precondition = 0;
		forceRazerPanelPowerConfig = 0;
		return originalHwSetPanelPower(controller, state);
	}
	logPanelPowerRegisters(controller,
		state == 0 ? "hwSetPanelPower(state=0) intercepted" :
		state == 2 ? "hwSetPanelPower(state=2) before" :
		"hwSetPanelPower(other) before");
	const bool nativeS3PanelOn = state == 2 &&
		systemTerminationPending == 0 &&
		panelNeedsD0Precondition != 0 && cmlPhyReinitArmed != 0 &&
		targetCMLIGPUValidated != 0;
	if (nativeS3PanelOn) {
		(void)restoreSouthChickenHotPlugDurationBeforePanelOn(controller,
			"native-hwSetPanelPower-before-hpd-controls");
	}
	if (state == 2) {
		if (southChickenLock != nullptr)
			IOLockLock(southChickenLock);
		southChickenPanelOffCompleted = 0;
		OSSynchronizeIO();
		if (southChickenLock != nullptr)
			IOLockUnlock(southChickenLock);
	}
	if (nativeS3PanelOn) {
		(void)restoreWindowsEDPHotPlugControls(controller,
			"native-hwSetPanelPower-before-on",
			"WakeNativeHPDBeforeControlRestore",
			"WakeNativeHPDAfterControlRestore",
			"WakeNativeHPDControlStatus");
		publishRegister32("WakeSouthChickenDurationAfterHPDControls",
			callReadRegister32(controller, kSouthChicken1));
	}
	if (state == 0) {
		// Recheck after the diagnostic prefix.  A shutdown notification may race a
		// display-only state=0 that entered just before the priority gate flipped.
		if (systemTerminationPending != 0) {
			panelNeedsD0Precondition = 0;
			forceRazerPanelPowerConfig = 0;
			return originalHwSetPanelPower(controller, state);
		}
		panelNeedsD0Precondition = 1;
		if (systemSleepPending != 0) {
			// During real S3, an orderly panel power-down must finish before the
			// firmware removes the rail.  This is intentionally different from a
			// display-only blank, where retaining receiver power is required.
			publishDisplayRegisterSnapshot(controller, "SleepPre");
			publishHPDRegisterSnapshot(controller, "SleepPreHPDRegisters");
			(void)captureColdSouthChicken1(
				"real-system-sleep-working-pre-off");
			publishRegister32("SleepPreSouthChicken1",
				callReadRegister32(controller, kSouthChicken1));
			const bool ddiTableCaptured = captureDDIBufferTranslation(controller,
				"real-system-sleep");
			savedDisplayIOBalanceLeg = callReadRegister32(controller,
				kDisplayIOBalanceLeg);
			const auto savedAE =
				savedDisplayIOBalanceLeg & kBalanceLegControlledAE;
			const bool bmuSnapshotValid =
				savedDisplayIOBalanceLeg != 0xFFFFFFFFU &&
				(savedAE == kBalanceLegQuiescentAE ||
				 savedAE == kBalanceLegIboost1AE ||
				 savedAE == kBalanceLegIboostDisabledAE);
			savedDisplayIOBalanceLegValid = bmuSnapshotValid ? 1 : 0;
			publishRegister32("Saved_BMU", savedDisplayIOBalanceLeg);
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty("BMURestoreStatus",
					bmuSnapshotValid ?
					"armed-from-sleep-observed-working-ae-state" :
					"not-armed-unexpected-sleep-pre-ae-state");
			SYSLOG("power", "real system sleep: saved DDI table=%u and Gen9 balance-leg register 0x%08X valid-observed-working-AE=%u; allowing native clean panel power-off before S3 rail removal",
				ddiTableCaptured, savedDisplayIOBalanceLeg, bmuSnapshotValid);
			const auto result = originalHwSetPanelPower(controller, state);
			const auto afterOff = callReadRegister32(controller,
				kPanelPowerStatus);
			const bool panelOffVerified = afterOff != 0xFFFFFFFFU &&
				(afterOff & (kPanelPowerOn | kPanelPowerSequenceMask)) == 0;
			if (southChickenLock != nullptr)
				IOLockLock(southChickenLock);
			southChickenPanelOffCompleted = panelOffVerified &&
				systemSleepPending != 0 ? 1 : 0;
			if (southChickenLock != nullptr)
				IOLockUnlock(southChickenLock);
			publishRegister32("SleepPanelOffResult", result);
			publishRegister32("SleepPanelOffPPStatus", afterOff);
			publishRegister32("SleepPanelOffVerified",
				panelOffVerified ? 1 : 0);
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty(
					"SleepPanelOffVerificationStatus", panelOffVerified ?
					"native-panel-off-pp-status-verified" :
					"native-panel-off-pp-status-not-off-refclk-set-refused");
			logPanelPowerRegisters(controller, "hwSetPanelPower(state=0) clean system-sleep off after");
			return result;
		}
		// For display-only sleep the caller has already put the sink in D3 and
		// disabled the DDI video path, so the pixels are dark.  Retain only the
		// panel electronics and avoid the known OLED display-wake failure.
		// Recheck immediately before suppression and do no logging in this final
		// window, so a concurrent termination gets the native physical panel-off.
		OSSynchronizeIO();
		if (systemTerminationPending != 0) {
			panelNeedsD0Precondition = 0;
			forceRazerPanelPowerConfig = 0;
			return originalHwSetPanelPower(controller, state);
		}
		return 0;
	}
	const auto result = originalHwSetPanelPower(controller, state);
	if (nativeS3PanelOn) {
		publishRegister32("WakeSouthChickenDurationAfterNativePanelOn",
			callReadRegister32(controller, kSouthChicken1));
		publishHPDRegisterSnapshot(controller,
			"WakeNativeHPDAfterPanelOn");
		wakeHPDReadyAfterNativePanelOn =
			waitForPortAHPDAfterNativePanelOn(controller) ? 1 : 0;
		publishHPDRegisterSnapshot(controller,
			"WakeNativeHPDAfterReadyWait");
	}
	logPanelPowerRegisters(controller,
		state == 2 ? "hwSetPanelPower(state=2) after" :
		"hwSetPanelPower(other) after");
	return result;
}

static void wrapConfigureBufferTranslation(void *controller,
	void *framebuffer, void *displayPath) {
	originalConfigureBufferTranslation(controller, framebuffer, displayPath);

	const auto mode = bufferTranslationMode;
	if (mode == 1 && savedDDIBufferTranslationValid == 0) {
		captureDDIBufferTranslation(controller, "cold-working-LightUpEDP");
	} else if (mode == 2) {
		const bool restored = restoreDDIBufferTranslation(controller);
		SYSLOG("ddi", "Apple ConfigureBufferTranslation completed during S3 wake; exact sleep/cold table restore verified=%u",
			restored);
	}
}

// Replacement for only LinkTraining Phase 1's IOSleep immediately before the
// DPCD 0x202 lane-status read.  Preserve Tahoe's native duration everywhere
// except a committed S3 wake on the validated Razer/Samsung path, and require
// the cold-working receiver snapshot to contain the exact interval byte seen
// in both macOS and the successful Windows trace.
static void sleepPhase1ClockRecoveryWindowsInterval(uint32_t milliseconds) {
	const bool shouldExtend =
		trainingDiagnosticsArmed != 0 &&
		panelNeedsD0Precondition != 0 &&
		targetCMLIGPUValidated != 0 &&
		milliseconds == kApplePhase1ClockRecoveryDelayMs &&
		coldWorkingTrainingIntervalMatchesEvidence();
	const auto appliedMilliseconds = shouldExtend ?
		kWindowsPhase1ClockRecoveryDelayMs : milliseconds;

	if (trainingDiagnosticsArmed != 0 && panelNeedsD0Precondition != 0) {
		const auto count = ++phase1ClockRecoveryDelayCount;
		publishRegister32("WakePhase1ClockRecoveryDelayCount", count);
		publishRegister32("WakePhase1ClockRecoveryNativeDelayMs",
			milliseconds);
		publishRegister32("WakePhase1ClockRecoveryAppliedDelayMs",
			appliedMilliseconds);
		publishRegister32("WakePhase1ClockRecoveryDPCDInterval",
			kSamsungTrainingAuxReadInterval);
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"WakePhase1ClockRecoveryDelayStatus", shouldExtend ?
					"extended-apple-8ms-to-windows-16ms-before-status-read" :
					"native-forwarded-strict-gate-not-satisfied");
		SYSLOG("training", "Phase-1 status wait #%u native=%ums applied=%ums DPCD[00E]=%02X extended=%u",
			count, milliseconds, appliedMilliseconds,
			kSamsungTrainingAuxReadInterval, shouldExtend);
	}

	IOSleep(appliedMilliseconds);
}

static uint32_t wrapCheckClockRecovery(void *controller, uint8_t lane,
	void *linkTrainingStatus) {
	const auto result = originalCheckClockRecovery(controller, lane,
		linkTrainingStatus);
	if (trainingDiagnosticsArmed == 0 || callReadRegister32 == nullptr)
		return result;

	const auto check = ++trainingCheckCount;
	const auto ddiBuffer = callReadRegister32(controller, kDDIBufferControlA);
	const auto transportControl = callReadRegister32(controller,
		kDPTransportControlA);
	const auto transportStatus = callReadRegister32(controller,
		kDPTransportStatusA);
	const auto portClock = callReadRegister32(controller, kPortClockSelectA);
	const auto balanceLeg = callReadRegister32(controller,
		kDisplayIOBalanceLeg);
	uint32_t translations[kDDIBufferTranslationDwordCount] {};
	readDDIBufferTranslation(controller, translations);

	uint32_t sinkLaneStatus = 0;
	if (linkTrainingStatus != nullptr) {
		const auto bytes = static_cast<const uint8_t *>(linkTrainingStatus);
		sinkLaneStatus = static_cast<uint32_t>(bytes[0]) |
			(static_cast<uint32_t>(bytes[1]) << 8);
	}

	publishRegister32("TrainingCheckCount", check);
	publishRegister32("TrainingLastLane", lane);
	publishRegister32("TrainingLastResult", result);
	publishRegister32("TrainingLastSinkLaneStatus", sinkLaneStatus);
	publishRegister32("TrainingLast_DDI_BUF_CTL_A", ddiBuffer);
	publishRegister32("TrainingLast_DP_TP_CTL_A", transportControl);
	publishRegister32("TrainingLast_DP_TP_STATUS_A", transportStatus);
	publishRegister32("TrainingLast_PORT_CLK_SEL_A", portClock);
	publishRegister32("TrainingLast_BMU", balanceLeg);
	publishRegisterData("TrainingLast_DDI_BUF_TRANS_A", translations,
		kDDIBufferTranslationDwordCount);

	SYSLOG("training", "CR check=%u lane=%u result=0x%08X sink=%04X DDI_BUF=%08X select=%u TP_CTL=%08X TP_STATUS=%08X CLK=%08X BMU=%08X",
		check, lane, result, sinkLaneStatus, ddiBuffer,
		(ddiBuffer >> 24) & 0xF, transportControl, transportStatus,
		portClock, balanceLeg);
	return result;
}

// Replacement for the first DDI_BUF_CTL_A enable in regular LinkTraining.
// This is earlier than the Phase-1 voltage-adjustment call used by V47: when
// it returns, Tahoe's very next hardware operation is DPCD 0x102=0x21.  Keep
// cold boot and display-only blanking byte-for-byte native; only a committed
// S3 wake receives the bounded source-carrier verification and repair.
static void writeInitialDDIBufferControlAndVerify(void *controller,
	uint32_t address, uint32_t value) {
	if (callWriteRegister32 == nullptr)
		return;
	const bool shouldVerify = trainingDiagnosticsArmed != 0 &&
		panelNeedsD0Precondition != 0 &&
		targetCMLIGPUValidated != 0 &&
		v61PatchFullyArmed != 0 &&
		systemTerminationPending == 0 &&
		address == kDDIBufferControlA &&
		(value & kDDIBufferEnable) != 0 &&
		callReadRegister32 != nullptr && callWriteRegister32 != nullptr;
	if (!shouldVerify) {
		callWriteRegister32(controller, address, value);
		return;
	}
	bool captureFirstTrainingDuration = false;
	if (southChickenLock != nullptr) {
		IOLockLock(southChickenLock);
		if (southChickenDurationFirstTrainingSampled == 0) {
			southChickenDurationFirstTrainingSampled = 1;
			captureFirstTrainingDuration = true;
		}
		IOLockUnlock(southChickenLock);
	}
	if (captureFirstTrainingDuration) {
		const auto southChickenAtFirstSource =
			callReadRegister32(controller, kSouthChicken1);
		publishRegister32("WakeSouthChickenDurationAtFirstTrainingSource",
			southChickenAtFirstSource);
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"WakeSouthChickenDurationAtFirstTrainingStatus",
				(southChickenAtFirstSource &
					kChassisClockRequestDurationMask) ==
					kChassisClockRequestDurationColdExpected ?
				"duration-f-preserved-through-first-training-source" :
				"duration-not-f-at-first-training-source");
	}

	const auto attempt = ++initialCarrierActivationCount;
	const auto transportBefore = callReadRegister32(controller,
		kDPTransportControlA);
	const auto transportExpected =
		(transportBefore | kDPTransportEnable) & ~kDPTransportPatternMask;
	const bool transportWasPattern1 =
		(transportBefore & kDPTransportEnable) != 0 &&
		(transportBefore & kDPTransportPatternMask) ==
			kDPTransportPattern1;
	if (!transportWasPattern1) {
		callWriteRegister32(controller, kDPTransportControlA,
			transportExpected);
		OSSynchronizeIO();
		++initialCarrierRepairCount;
	}

	const auto transportAfterRepair = callReadRegister32(controller,
		kDPTransportControlA);
	const auto ddiBefore = callReadRegister32(controller, address);
	const bool entryWasDisabled =
		(ddiBefore & kDDIBufferEnable) == 0;
	const bool entryWasIdle =
		(ddiBefore & kDDIBufferIdle) != 0;
	LinuxKBLIBoostResult linuxIBoostResult {};
	if (entryWasDisabled && entryWasIdle)
		linuxIBoostResult =
			applyLinuxKBLDPTrainingIBoost(controller, value);
	const bool linuxIBoostWasVerified =
		linuxKBLIBoostVerified(linuxIBoostResult);
	callWriteRegister32(controller, address, value);
	OSSynchronizeIO();
	const auto ddiAfterWrite = callReadRegister32(controller, address);
	IODelay(kDDIBufferActivationSettleUs);
	OSSynchronizeIO();
	const auto ddiAfterSettle = callReadRegister32(controller, address);
	const auto transportAfterSettle = callReadRegister32(controller,
		kDPTransportControlA);
	const auto transportStatus = callReadRegister32(controller,
		kDPTransportStatusA);
	const auto dpllControl2 = callReadRegister32(controller, kDPLLControl2);
	const auto dpllStatus = callReadRegister32(controller, kDPLLStatus);
	const auto portClock = callReadRegister32(controller, kPortClockSelectA);
	const auto powerWell = callReadRegister32(controller, kPowerWellControl2);
	publishLinuxKBLDPTrainingIBoost(linuxIBoostResult,
		"initial-phase1-source-before-ddi-enable");

	const bool transportReady =
		(transportAfterSettle & kDPTransportEnable) != 0 &&
		(transportAfterSettle & kDPTransportPatternMask) ==
			kDPTransportPattern1;
	const bool ddiReady = (ddiAfterSettle & kDDIBufferEnable) != 0;
	const bool clockReady =
		(dpllStatus & kDPLL0Lock) != 0 &&
		(dpllControl2 & kDPLLControl2PortAClockOff) == 0 &&
		(dpllControl2 & kDPLLControl2PortAOverride) != 0;

	publishRegister32("WakeInitialCarrierAttempt", attempt);
	publishRegister32("WakeInitialCarrierRepairCount",
		initialCarrierRepairCount);
	publishRegister32("WakeInitialCarrier_DP_TP_CTL_Before",
		transportBefore);
	publishRegister32("WakeInitialCarrier_DP_TP_CTL_AfterRepair",
		transportAfterRepair);
	publishRegister32("WakeInitialCarrier_DP_TP_CTL_AfterSettle",
		transportAfterSettle);
	publishRegister32("WakeInitialCarrier_DP_TP_STATUS", transportStatus);
	publishRegister32("WakeInitialCarrier_DDI_BUF_CTL_Before", ddiBefore);
	publishRegister32("WakeInitialCarrierEntryDisabled",
		entryWasDisabled ? 1 : 0);
	publishRegister32("WakeInitialCarrierEntryIdle",
		entryWasIdle ? 1 : 0);
	publishRegister32("WakeInitialCarrierLinuxIBoostVerified",
		linuxIBoostWasVerified ? 1 : 0);
	publishRegister32("WakeInitialCarrier_DDI_BUF_CTL_Requested", value);
	publishRegister32("WakeInitialCarrier_DDI_BUF_CTL_AfterWrite",
		ddiAfterWrite);
	publishRegister32("WakeInitialCarrier_DDI_BUF_CTL_AfterSettle",
		ddiAfterSettle);
	publishRegister32("WakeInitialCarrier_DPLL_CTRL2", dpllControl2);
	publishRegister32("WakeInitialCarrier_DPLL_STATUS", dpllStatus);
	publishRegister32("WakeInitialCarrier_PORT_CLK_SEL_A", portClock);
	publishRegister32("WakeInitialCarrier_PWR_WELL_CTL2", powerWell);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeInitialCarrierStatus",
			!entryWasDisabled || !entryWasIdle ?
			"unexpected-nonidle-entry-native-ddi-write-forwarded" :
			(!linuxIBoostWasVerified ?
			 "linux-kbl-iboost-not-verified-native-ddi-write-forwarded" :
			 (transportReady && ddiReady && clockReady ?
			  "pattern1-linux-kbl-iboost-ddi-dpll-ready-before-sink-arming" :
			  "source-carrier-not-ready-before-sink-arming")));
	SYSLOG("training", "S3 initial source carrier #%u before sink Pattern-1 arm: TP=%08X repaired=%u after=%08X settled=%08X TP_STATUS=%08X DDI=%08X entry-disabled/idle=%u/%u requested=%08X linux-iboost=%u write=%08X settled=%08X DPLL_CTRL2=%08X DPLL_STATUS=%08X CLK=%08X PWR=%08X ready=%u/%u/%u",
		attempt, transportBefore, !transportWasPattern1,
		transportAfterRepair, transportAfterSettle, transportStatus,
		ddiBefore, entryWasDisabled, entryWasIdle, value,
		linuxIBoostWasVerified, ddiAfterWrite, ddiAfterSettle, dpllControl2,
		dpllStatus, portClock, powerWell, transportReady, ddiReady,
		clockReady);
}

// Replacement target for the Phase-1 DDI_BUF_CTL source-strength write.  Its
// ABI is identical to WriteRegister32, so all arguments and return behaviour
// remain native.  The original writer is called through its symbol entry;
// WhateverGreen may route that entry later, preserving its backlight fix.
static void writeDDIBufferControlAndSettle(void *controller, uint32_t address,
	uint32_t value) {
	if (callWriteRegister32 == nullptr)
		return;
	const bool shouldSettle = trainingDiagnosticsArmed != 0 &&
		panelNeedsD0Precondition != 0 &&
		targetCMLIGPUValidated != 0 && v61PatchFullyArmed != 0 &&
		systemTerminationPending == 0 &&
		address == kDDIBufferControlA &&
		(value & kDDIBufferEnable) != 0;
	const auto before = shouldSettle && callReadRegister32 != nullptr ?
		callReadRegister32(controller, address) : 0;

	// Match Linux's critical write pair: update the A/E I_boost state selected
	// by the KBL table, then immediately forward Tahoe's native DDI_BUF_CTL
	// selector unchanged.  All diagnostic publication is deferred until after
	// the native settling window.
	LinuxKBLIBoostResult linuxIBoostResult {};
	if (shouldSettle)
		linuxIBoostResult =
			applyLinuxKBLDPTrainingIBoost(controller, value);
	const bool linuxIBoostWasVerified =
		linuxKBLIBoostVerified(linuxIBoostResult);
	callWriteRegister32(controller, address, value);
	if (!shouldSettle || callReadRegister32 == nullptr)
		return;

	const auto nativeSourceCount = ++phase1NativeSourceWriteCount;
	OSSynchronizeIO();
	const auto afterWrite = callReadRegister32(controller, address);
	IODelay(kDDIBufferActivationSettleUs);
	OSSynchronizeIO();
	const auto afterSettle = callReadRegister32(controller, address);
	const auto transportAtSourceWrite = callReadRegister32(controller,
		kDPTransportControlA);
	const auto transportStatusAtSourceWrite = callReadRegister32(controller,
		kDPTransportStatusA);
	const auto count = ++ddiBufferActivationSettleCount;
	publishLinuxKBLDPTrainingIBoost(linuxIBoostResult,
		"phase1-source-strength-before-ddi-write");
	publishRegister32("WakeDDIBufferEnableBefore", before);
	publishRegister32("WakeDDIBufferEnableRequested", value);
	publishRegister32("WakeDDIBufferEnableAfterWrite", afterWrite);
	publishRegister32("WakePhase1Last_DP_TP_CTL_A",
		transportAtSourceWrite);
	publishRegister32("WakePhase1Last_DP_TP_STATUS_A",
		transportStatusAtSourceWrite);
	publishRegister32("WakeDDIBufferActivationSettleUs",
		kDDIBufferActivationSettleUs);
	publishRegister32("WakeDDIBufferActivationSettleCount", count);
	publishRegister32("WakePhase1NativeSourceWriteCount", nativeSourceCount);
	publishRegister32("WakePhase1NativeSourceValue", value);
	publishRegister32("WakePhase1NativeSourceSelector",
		(value & kDDITrainingSelectorMask) >> 24);
	publishRegister32("WakePhase1LinuxIBoostVerified",
		linuxIBoostWasVerified ? 1 : 0);
	publishRegister32("WakeDDIBufferEnableAfterSettle", afterSettle);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
	{
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakeDDIBufferActivationStatus",
			(afterSettle & kDDIBufferEnable) != 0 ?
			"enabled-and-settled-before-dpcd-pattern" :
			"enable-bit-dropped-during-settle");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakePhase1NativeSourceStatus",
			linuxIBoostWasVerified ?
			"native-ddi-selector-forwarded-with-linux-kbl-iboost" :
			"native-ddi-selector-forwarded-iboost-not-verified");
	}
	SYSLOG("training", "S3 native Phase-1 source #%u: before=%08X requested=%08X selector=%u linux-iboost=%u after-write=%08X after-%uus=%08X",
		nativeSourceCount, before, value,
		(value & kDDITrainingSelectorMask) >> 24,
		linuxIBoostWasVerified, afterWrite,
		kDDIBufferActivationSettleUs, afterSettle);
}

// ABI-identical target for Tahoe's separate Phase-2 DDI source-strength call.
// Do not add a new delay here: Linux changes I_boost, writes DDI_BUF_CTL and
// relies on the native channel-equalisation interval before reading status.
static void writePhase2DDIBufferControlWithLinuxKBLIBoost(void *controller,
	uint32_t address, uint32_t value) {
	if (callWriteRegister32 == nullptr)
		return;
	const bool shouldApply = trainingDiagnosticsArmed != 0 &&
		panelNeedsD0Precondition != 0 &&
		targetCMLIGPUValidated != 0 && v61PatchFullyArmed != 0 &&
		systemTerminationPending == 0 &&
		address == kDDIBufferControlA &&
		(value & kDDIBufferEnable) != 0;
	const auto before = shouldApply && callReadRegister32 != nullptr ?
		callReadRegister32(controller, address) : 0;
	LinuxKBLIBoostResult linuxIBoostResult {};
	if (shouldApply)
		linuxIBoostResult =
			applyLinuxKBLDPTrainingIBoost(controller, value);
	const bool linuxIBoostWasVerified =
		linuxKBLIBoostVerified(linuxIBoostResult);
	callWriteRegister32(controller, address, value);
	if (!shouldApply || callReadRegister32 == nullptr)
		return;

	OSSynchronizeIO();
	const auto after = callReadRegister32(controller, address);
	publishLinuxKBLDPTrainingIBoost(linuxIBoostResult,
		"phase2-source-strength-before-ddi-write");
	const auto count = ++phase2NativeSourceWriteCount;
	publishRegister32("WakePhase2NativeSourceWriteCount", count);
	publishRegister32("WakePhase2NativeSourceBefore", before);
	publishRegister32("WakePhase2NativeSourceValue", value);
	publishRegister32("WakePhase2NativeSourceSelector",
		(value & kDDITrainingSelectorMask) >> 24);
	publishRegister32("WakePhase2NativeSourceAfter", after);
	publishRegister32("WakePhase2LinuxIBoostVerified",
		linuxIBoostWasVerified ? 1 : 0);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakePhase2NativeSourceStatus", linuxIBoostWasVerified ?
			"native-ddi-selector-forwarded-with-linux-kbl-iboost" :
			"native-ddi-selector-forwarded-iboost-not-verified");
	SYSLOG("training", "S3 native Phase-2 source #%u: before=%08X requested=%08X selector=%u linux-iboost=%u after=%08X",
		count, before, value,
		(value & kDDITrainingSelectorMask) >> 24,
		linuxIBoostWasVerified, after);
}

// Diagnostic pass-through for only the Phase-1 LinkTraining WriteAUX call at
// DPCD 0x103-0x106.  V55 incorrectly substituted final Phase-2 bytes here.
// V57 preserves V56's single call to Apple's native writer with the original address,
// length, buffer and display path, then records the values without changing
// source or sink state.
static IOReturn writeNativePhase1LaneSetForS3(void *controller,
	void *framebuffer, uint32_t address, uint16_t length, void *buffer,
	void *displayPath) {
	if (callWriteAUX == nullptr)
		return kIOReturnNotReady;

	const auto nativeLaneSet = static_cast<const uint8_t *>(buffer);
	const bool shouldObserve =
		trainingDiagnosticsArmed != 0 &&
		panelNeedsD0Precondition != 0 &&
		targetCMLIGPUValidated != 0 &&
		isInternalEDPPath(framebuffer, displayPath) &&
		address == kDPCDTrainingLane0Set &&
		length == 4 && nativeLaneSet != nullptr;
	const bool exactNativeSwing2Pre0 = nativeLaneSet != nullptr && length == 4 &&
		nativeLaneSet[0] == kDPTrainingSwing2Pre0 &&
		nativeLaneSet[1] == kDPTrainingSwing2Pre0 &&
		nativeLaneSet[2] == kDPTrainingSwing2Pre0 &&
		nativeLaneSet[3] == kDPTrainingSwing2Pre0;
	const bool exactNativeSwing3Pre0 = nativeLaneSet != nullptr && length == 4 &&
		nativeLaneSet[0] == kDPTrainingSwing3Pre0 &&
		nativeLaneSet[1] == kDPTrainingSwing3Pre0 &&
		nativeLaneSet[2] == kDPTrainingSwing3Pre0 &&
		nativeLaneSet[3] == kDPTrainingSwing3Pre0;
	uint8_t observedLaneSet[4] {};
	if (shouldObserve) {
		for (uint32_t lane = 0; lane < 4; lane++)
			observedLaneSet[lane] = nativeLaneSet[lane];
	}
	const auto result = callWriteAUX(controller, framebuffer, address, length,
		buffer, displayPath);
	if (!shouldObserve)
		return result;

	const auto nativeCount = ++phase1NativeSinkWriteCount;
	if (exactNativeSwing2Pre0)
		++phase1NativeSink06Count;
	else if (exactNativeSwing3Pre0)
		++phase1NativeSink07Count;
	else
		++phase1NativeSinkOtherCount;
	const uint32_t packedLaneSet =
		static_cast<uint32_t>(observedLaneSet[0]) |
		(static_cast<uint32_t>(observedLaneSet[1]) << 8) |
		(static_cast<uint32_t>(observedLaneSet[2]) << 16) |
		(static_cast<uint32_t>(observedLaneSet[3]) << 24);
	publishRegister32("WakePhase1NativeSinkWriteCount", nativeCount);
	publishRegister32("WakePhase1NativeSink06Count",
		phase1NativeSink06Count);
	publishRegister32("WakePhase1NativeSink07Count",
		phase1NativeSink07Count);
	publishRegister32("WakePhase1NativeSinkOtherCount",
		phase1NativeSinkOtherCount);
	publishRegister32("WakePhase1NativeSinkLaneSet", packedLaneSet);
	publishRegister32("WakePhase1NativeSinkWriteResult",
		static_cast<uint32_t>(result));
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WakePhase1NativeSinkStatus", result == kIOReturnSuccess ?
				"native-lane-bytes-forwarded-unchanged" :
				"native-lane-set-write-failed");
	}
	SYSLOG("training", "S3 native Phase-1 sink #%u forwarded unchanged: DPCD 0x103-0x106=%02X/%02X/%02X/%02X result=0x%08X",
		nativeCount, observedLaneSet[0], observedLaneSet[1],
		observedLaneSet[2], observedLaneSet[3], result);
	return result;
}

// Intel's Gen9 DisplayPort enable sequence requires the port PLL and its DDI
// clock mapping to be active before DDI IO power is enabled.  V43/V44 rebuilt
// the shared DDIA/E IO well at the start of LightUpEDP, while S3 had left Port
// A's DPLL_CTRL2 override clear.  Apple only restores that mapping later in
// EnableClocks, so the final register snapshot looked correct even though the
// analog PHY had already been powered up without its source clock.
//
// LightUpEDP calls EnableClocks immediately before LinkTraining.  Synchronize
// ownership of the continuously-powered shared DDIA/E well only after Apple's
// native routine has restored and locked DPLL0, then restore the S3-volatile
// x4, translation, and A/E iBoost state.
// Cold boot, display-only blanking, non-eDP paths, and later modesets retain
// Apple's unmodified ordering.
static void wrapEnableClocks(void *controller, void *framebuffer,
	void *displayPath) {
	const bool shouldRebuild = panelNeedsD0Precondition != 0 &&
		cmlPhyReinitArmed != 0 && targetCMLIGPUValidated != 0 &&
		isInternalEDPPath(framebuffer, displayPath);

	if (shouldRebuild)
		publishDisplayRegisterSnapshot(controller,
			"WakeClockFirstBeforeNativeEnableClocks");

	originalEnableClocks(controller, framebuffer, displayPath);

	if (!shouldRebuild || callReadRegister32 == nullptr)
		return;

	publishDisplayRegisterSnapshot(controller,
		"WakeClockFirstAfterNativeEnableClocks");
	const auto dpllControl2 = callReadRegister32(controller, kDPLLControl2);
	const auto dpllStatus = callReadRegister32(controller, kDPLLStatus);
	const bool portAClockMapped =
		(dpllControl2 & kDPLLControl2PortAClockOff) == 0 &&
		(dpllControl2 & kDPLLControl2PortAOverride) != 0;
	const bool dpll0Locked = (dpllStatus & kDPLL0Lock) != 0;
	publishRegister32("WakeClockFirstNative_DPLL_CTRL2", dpllControl2);
	publishRegister32("WakeClockFirstNative_DPLL_STATUS", dpllStatus);

	if (!portAClockMapped || !dpll0Locked) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"WakeClockFirstPHYStatus",
				"refused-native-port-a-clock-not-mapped-or-locked");
		SYSLOG("phy", "clock-first DDIA/E ownership sync refused after native EnableClocks: DPLL_CTRL2=%08X DPLL_STATUS=%08X mapped=%u locked=%u",
			dpllControl2, dpllStatus, portAClockMapped, dpll0Locked);
		return;
	}

	const bool cmlPhyResult = syncCMLDDIAESourcePHYOwnership(controller);
	const bool translationResult = cmlPhyResult &&
		restoreDDIBufferTranslation(controller);
	const bool bmuResult = cmlPhyResult &&
		restoreDisplayIOBalanceLeg(controller);
	publishDisplayRegisterSnapshot(controller,
		"WakeClockFirstAfterDDIAEReinit");
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("WakeClockFirstPHYStatus",
			cmlPhyResult && translationResult && bmuResult ?
			"native-clock-mapped-then-ddiae-handoff-without-cycle-and-state-restored" :
			"clock-first-ddiae-handoff-or-state-restore-failed");
	SYSLOG("phy", "clock-first Gen9 DDIA/E ownership sync after native EnableClocks: DPLL_CTRL2=%08X DPLL_STATUS=%08X PHY=%u translations=%u BMU=%u",
		dpllControl2, dpllStatus, cmlPhyResult, translationResult, bmuResult);
}

static uint32_t wrapLightUpEDP(void *controller, void *framebuffer,
	void *displayPath, const void *timing) {
	const bool precondition = panelNeedsD0Precondition != 0;
	if (precondition)
		(void)restoreColdPchRawClock(controller,
			"LightUpEDP-precondition-fallback");
	DPCDSnapshotRecord wakeDPCDBeforeTraining[kDPCDSnapshotBlockCount] {};
	if (precondition) {
		publishDisplayRegisterSnapshot(controller, "WakePre");
		publishPchPanelPadSnapshot("WakePreTconCyclePchPanelPads");
		const bool lVddEnModeResult = restoreLVddEnNativeModeIfNeeded();
		publishPchPanelPadSnapshot("WakeAfterLVddEnModePchPanelPads");
		const bool ddiX4Result = restoreDDIAFourLaneCapability(controller,
			"LightUpEDP-precondition-fallback");
		publishPanelPowerSnapshot(controller,
			PanelPowerSnapshotStage::WakeResetInitial);

		uint32_t firstPanelOnResult = 0;
		bool firstPanelOnVerified = false;
		uint32_t firstD0Result = 0;
		uint32_t d3Result = 0;
		uint32_t panelOffResult = 0;
		bool panelOffVerified = false;
		bool ppsResult = false;
		uint32_t panelOnResult = 0;
		bool panelOnVerified = false;
		uint32_t d0Result = 0;
		bool firstReceiverReady = false;
		bool firstReceiverRetained = false;
		bool receiverRailCyclePerformed = false;
		bool receiverRetryWithoutCycle = false;
		uint32_t firstReceiverPowerReadResult =
			static_cast<uint32_t>(kIOReturnNotReady);
		uint32_t firstReceiverCapabilityReadResult =
			static_cast<uint32_t>(kIOReturnNotReady);
		uint8_t firstReceiverPower = 0xFF;
		bool firstHPDReady = false;
		bool firstPanelOnWasAlreadyLive = false;
		uint8_t firstReceiverCapabilities[kDPCDSnapshotBlockSize] {};
		if (wakePanelResetAttempted == 0) {
			wakePanelResetAttempted = 1;
			firstPanelOnWasAlreadyLive =
				wakeHPDReadyAfterNativePanelOn != 0;
			// Both possible first-on entry points use the same strict readiness
			// gate.  If Apple's earlier native panel-on already produced HPD, do
			// not power it a second time; otherwise issue exactly one first on.
			if (!firstPanelOnWasAlreadyLive) {
				(void)restoreWindowsEDPHotPlugControls(controller,
					"first-panel-on-readiness-check",
					"WakeCycleFirstHPDBeforeControlRestore",
					"WakeCycleFirstHPDAfterControlRestore",
					"WakeCycleFirstHPDControlStatus");
				forceRazerPanelPowerConfig = 1;
				ppsResult = restoreRazerPanelPowerSequence(controller);
				firstPanelOnResult = originalHwSetPanelPower(controller, 2);
				forceRazerPanelPowerConfig = 0;
			}
			publishPanelPowerSnapshot(controller,
				PanelPowerSnapshotStage::WakeResetFirstOn);
			publishHPDRegisterSnapshot(controller,
				"WakeCycleFirstHPDAfterPanelOn");
			publishPchPanelPadSnapshot("WakeFirstPanelOnPchPanelPads");
			const auto afterFirstOn = callReadRegister32(controller,
				kPanelPowerStatus);
			firstPanelOnVerified = afterFirstOn != 0xFFFFFFFFU &&
				(afterFirstOn & (kPanelPowerOn |
				kPanelPowerSequenceMask)) == kPanelPowerOn;

			IOSleep(kSamsungPostPanelOnAUXDelayMs);
			firstD0Result = callSetDPPowerState(controller, framebuffer, 1,
				displayPath);
			IOSleep(kSamsungPostD0SettleMs);
			firstHPDReady = waitForPortAHPDAfterNativePanelOn(controller);
			wakeHPDReadyAfterNativePanelOn = firstHPDReady ? 1 : 0;
			if (callReadAUX != nullptr &&
				isInternalEDPPath(framebuffer, displayPath)) {
				firstReceiverPowerReadResult = static_cast<uint32_t>(
					callReadAUX(controller, framebuffer, kDPCDSetPower, 1,
						&firstReceiverPower, displayPath));
				firstReceiverCapabilityReadResult = static_cast<uint32_t>(
					callReadAUX(controller, framebuffer, 0,
						static_cast<uint16_t>(sizeof(firstReceiverCapabilities)),
						firstReceiverCapabilities, displayPath));
			}
			// PPS cached-field restoration is retained as a diagnostic, but it
			// is not receiver readiness.  A verified rail-on, successful D0,
			// live HPD, DPCD power read and credible capability block are the
			// direct evidence needed to preserve Windows's one-panel-on path.
			firstReceiverReady = firstPanelOnVerified &&
				firstD0Result == 0 && firstHPDReady &&
				firstReceiverPowerReadResult ==
					static_cast<uint32_t>(kIOReturnSuccess) &&
				(firstReceiverPower & 0x07) == kDPCDSetPowerD0 &&
				firstReceiverCapabilityReadResult ==
					static_cast<uint32_t>(kIOReturnSuccess) &&
				firstReceiverCapabilities[0] != 0 &&
				firstReceiverCapabilities[0] != 0xFF;
			publishRegister32("WakeFirstPPSRestoreResult",
				ppsResult ? 1 : 0);
			publishRegister32("WakeFirstReceiverHPDReady",
				firstHPDReady ? 1 : 0);
			publishRegister32("WakeFirstReceiverPowerReadResult",
				firstReceiverPowerReadResult);
			publishRegister32("WakeFirstReceiverPowerValue",
				firstReceiverPower);
			publishRegister32("WakeFirstReceiverCapabilityReadResult",
				firstReceiverCapabilityReadResult);
			publishRegister32("WakeFirstReceiverDPCDRevision",
				firstReceiverCapabilities[0]);
			publishRegister32("WakeFirstReceiverReady",
				firstReceiverReady ? 1 : 0);

			if (firstReceiverReady) {
				firstReceiverRetained = true;
				d0Result = firstD0Result;
				publishRegister32("WakePanelResetFirstOnResult",
					firstPanelOnResult);
				publishRegister32("WakePanelResetFirstD0Result", firstD0Result);
				publishRegister32("WakePanelResetD0Result", d0Result);
				if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty(
					"WakePanelReceiverResetStatus",
					firstPanelOnWasAlreadyLive ?
					"native-panel-on-d0-hpd-aux-ready-rail-cycle-skipped" :
					"first-panel-on-d0-hpd-aux-ready-second-rail-cycle-skipped");
			SYSLOG("hpd", "first S3 receiver state retained for Windows-equivalent ordering: native-already-on=%u on=0x%08X verified=%u PPS=%u D0=0x%08X HPD=%u DPCD600=%02X/%08X caps-rev=%02X/%08X",
				firstPanelOnWasAlreadyLive, firstPanelOnResult,
				firstPanelOnVerified, ppsResult,
					firstD0Result, firstHPDReady, firstReceiverPower,
					firstReceiverPowerReadResult, firstReceiverCapabilities[0],
					firstReceiverCapabilityReadResult);
			} else {
				firstReceiverRetained = false;
				receiverRailCyclePerformed = true;
				d3Result = callSetDPPowerState(controller, framebuffer, 2,
					displayPath);
				IOSleep(kSamsungD3ToRailOffDelayMs);
				publishPanelPowerSnapshot(controller,
					PanelPowerSnapshotStage::WakeResetAfterD3Delay);
				publishPchPanelPadSnapshot("WakeAfterD3DelayPchPanelPads");

				panelOffResult = originalHwSetPanelPower(controller, 0);
				publishPanelPowerSnapshot(controller,
					PanelPowerSnapshotStage::WakeResetAfterOff);
				publishPchPanelPadSnapshot("WakePanelRailOffPchPanelPads");
				const auto afterOff = callReadRegister32(controller,
					kPanelPowerStatus);
				panelOffVerified = afterOff != 0xFFFFFFFFU &&
					(afterOff & (kPanelPowerOn |
					kPanelPowerSequenceMask)) == 0;
				IOSleep(kSamsungRailOffMinimumMs);

				(void)restoreWindowsEDPHotPlugControls(controller,
					"fallback-cycle-second-panel-on",
					"WakeCycleSecondHPDBeforeControlRestore",
					"WakeCycleSecondHPDAfterControlRestore",
					"WakeCycleSecondHPDControlStatus");
				forceRazerPanelPowerConfig = 1;
				ppsResult = restoreRazerPanelPowerSequence(controller);
				panelOnResult = originalHwSetPanelPower(controller, 2);
				forceRazerPanelPowerConfig = 0;
				publishPanelPowerSnapshot(controller,
					PanelPowerSnapshotStage::WakeResetAfterOn);
				publishHPDRegisterSnapshot(controller,
					"WakeCycleSecondHPDAfterPanelOn");
				publishPchPanelPadSnapshot("WakeSecondPanelOnPchPanelPads");
				const auto afterOn = callReadRegister32(controller,
					kPanelPowerStatus);
				panelOnVerified = afterOn != 0xFFFFFFFFU &&
					(afterOn & (kPanelPowerOn |
					kPanelPowerSequenceMask)) == kPanelPowerOn;
				IOSleep(kSamsungPostPanelOnAUXDelayMs);
				d0Result = callSetDPPowerState(controller, framebuffer, 1,
					displayPath);
				IOSleep(kSamsungPostD0SettleMs);

				publishRegister32("WakePanelResetFirstOnResult",
					firstPanelOnResult);
				publishRegister32("WakePanelResetFirstD0Result", firstD0Result);
				publishRegister32("WakePanelResetD3Result", d3Result);
				publishRegister32("WakePanelResetOffResult", panelOffResult);
				publishRegister32("WakePanelResetOnResult", panelOnResult);
				publishRegister32("WakePanelResetD0Result", d0Result);
				if (RazerOLEDWakeFix_selfInstance != nullptr)
					RazerOLEDWakeFix_selfInstance->setProperty(
						"WakePanelReceiverResetStatus",
						d3Result == 0 && panelOffVerified && ppsResult && panelOnVerified &&
						d0Result == 0 ?
						"first-receiver-not-ready-fallback-cycle-completed" :
						"first-receiver-not-ready-fallback-cycle-failed");
				SYSLOG("power", "first receiver not ready; bounded Samsung fallback: first-on=0x%08X verified=%u first-D0=0x%08X HPD=%u DPCD600=%02X/%08X caps=%08X D3=0x%08X off=0x%08X verified=%u second-on=0x%08X verified=%u second-D0=0x%08X",
					firstPanelOnResult, firstPanelOnVerified, firstD0Result,
					firstHPDReady, firstReceiverPower,
					firstReceiverPowerReadResult,
					firstReceiverCapabilityReadResult, d3Result,
					panelOffResult, panelOffVerified, panelOnResult,
					panelOnVerified, d0Result);
			}
		} else {
			receiverRetryWithoutCycle = true;
			if (RazerOLEDWakeFix_selfInstance != nullptr)
				RazerOLEDWakeFix_selfInstance->setProperty(
					"WakePanelReceiverResetRetry",
					"skipped-already-cycled-this-s3");
			d0Result = callSetDPPowerState(controller, framebuffer, 1,
				displayPath);
			publishRegister32("WakePanelResetD0Result", d0Result);
			IOSleep(kSamsungPostD0SettleMs);
		}

		const char *receiverPath = firstReceiverRetained ?
			"Windows-HPD-live-no-rail-cycle" :
			(receiverRailCyclePerformed ? "bounded-Samsung-fallback-cycle" :
			(receiverRetryWithoutCycle ? "retry-D0-no-rail-cycle" :
			"HPD-live-D0-write-failed-no-rail-cycle"));
		SYSLOG("power", "wake precondition: L_VDDEN-native-mode=%u early-DDIA-x4=%u final-DPCD-D0=0x%08X receiver-path=%s",
			lVddEnModeResult, ddiX4Result, d0Result,
			receiverPath);
		const bool psrExitResult = exitPSRBeforeWakeLinkTraining(controller,
			framebuffer, displayPath);
		SYSLOG("psr", "wake precondition: bounded source/sink PSR exit before clock-first PHY reconstruction and link training result=%u",
			psrExitResult);
		SYSLOG("phy", "wake precondition: DDIA/E reconstruction deferred until Apple's native EnableClocks has restored the Port A DPLL mapping");
		const bool dpcdBeforeResult = captureDPCDSnapshot(controller,
			framebuffer, displayPath, wakeDPCDBeforeTraining,
			"WakeDPCDBeforeTraining");
		compareWakeDPCDWithCold(wakeDPCDBeforeTraining, "before-training");
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"WakeDPCDBeforeTrainingStatus",
				dpcdBeforeResult ? "essential-blocks-readable" :
					"one-or-more-essential-blocks-unavailable");
	}

	bufferTranslationMode = precondition ? 2 :
		(savedDDIBufferTranslationValid == 0 ? 1 : 0);
	trainingCheckCount = 0;
	phase1ClockRecoveryDelayCount = 0;
	nativeEDPConfigurationInterceptCount = 0;
	nativeLinkRateSelectInterceptCount = 0;
	initialCarrierActivationCount = 0;
	initialCarrierRepairCount = 0;
	phase1NativeSourceWriteCount = 0;
	phase1NativeSinkWriteCount = 0;
	phase1NativeSink06Count = 0;
	phase1NativeSink07Count = 0;
	phase1NativeSinkOtherCount = 0;
	linuxKBLIBoostApplyCount = 0;
	linuxKBLIBoostVerifiedCount = 0;
	linuxKBLIBoostRefusedCount = 0;
	phase2NativeSourceWriteCount = 0;
	trainingDiagnosticsArmed = precondition ? 1 : 0;
	const auto result = originalLightUpEDP(controller, framebuffer, displayPath,
		timing);
	trainingDiagnosticsArmed = 0;
	bufferTranslationMode = 0;
	if (!precondition && result == 0 &&
		isInternalEDPPath(framebuffer, displayPath)) {
		(void)captureColdPchRawClock(controller,
			"successful-cold-LightUpEDP");
		(void)captureColdSouthChicken1("successful-cold-LightUpEDP");
	}
	if (precondition) {
		DPCDSnapshotRecord wakeDPCDAfterTraining[kDPCDSnapshotBlockCount] {};
		const bool dpcdAfterResult = captureDPCDSnapshot(controller,
			framebuffer, displayPath, wakeDPCDAfterTraining,
			"WakeDPCDAfterTraining");
		compareWakeDPCDWithCold(wakeDPCDAfterTraining, "after-training");
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"WakeDPCDAfterTrainingStatus",
				dpcdAfterResult ? "essential-blocks-readable" :
					"one-or-more-essential-blocks-unavailable");
		publishDisplayRegisterSnapshot(controller, "WakePost");
		publishPchPanelPadSnapshot("WakePostLightUpPchPanelPads");
		panelNeedsD0Precondition = result == 0 ? 0 : 1;
		SYSLOG("power", "preconditioned LightUpEDP returned 0x%08X; next-precondition=%u",
			result, panelNeedsD0Precondition);
	} else if (result == 0 && savedColdDPCDValid == 0 &&
		isInternalEDPPath(framebuffer, displayPath)) {
		publishPSRRegisterSnapshot(controller, PSRSnapshotStage::ColdWorking);
		publishHPDRegisterSnapshot(controller, "ColdWorkingHPDRegisters");
		const bool captured = captureDPCDSnapshot(controller, framebuffer,
			displayPath, savedColdDPCD, "ColdWorkingDPCD");
		OSSynchronizeIO();
		savedColdDPCDValid = captured ? 1 : 0;
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty(
				"ColdWorkingDPCDStatus", captured ?
					"captured-essential-blocks-after-successful-link" :
					"essential-capture-incomplete-not-eligible-for-comparison");
	}
	return result;
}

static bool findPatternOnce(const uint8_t *pattern, size_t patternSize,
	const uint8_t *data, size_t dataSize, size_t &firstOffset) {
	if (pattern == nullptr || data == nullptr || patternSize == 0 || dataSize < patternSize)
		return false;

	if (!KernelPatcher::findPattern(pattern, nullptr, patternSize, data, dataSize, &firstOffset))
		return false;

	const size_t secondSearchOffset = firstOffset + 1;
	if (secondSearchOffset >= dataSize || dataSize - secondSearchOffset < patternSize)
		return true;

	size_t secondOffset = 0;
	return !KernelPatcher::findPattern(pattern, nullptr, patternSize,
		data + secondSearchOffset, dataSize - secondSearchOffset, &secondOffset);
}

static bool validateTargetCMLIGPU() {
	auto matching = IOService::serviceMatching("IOPCIDevice");
	if (matching == nullptr) {
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("TargetIGPUStatus",
				"refused-no-iopci-matching-dictionary");
		return false;
	}

	auto iterator = IOService::getMatchingServices(matching);
	if (iterator == nullptr) {
		matching->release();
		if (RazerOLEDWakeFix_selfInstance != nullptr)
			RazerOLEDWakeFix_selfInstance->setProperty("TargetIGPUStatus",
				"refused-no-iopci-iterator");
		return false;
	}

	bool foundIGPU = false;
	bool foundPCH = false;
	IOPCIDevice *targetIGPUService = nullptr;
	uint32_t foundIGPUVendor = 0;
	uint32_t foundIGPUDevice = 0;
	uint32_t foundPCHVendor = 0;
	uint32_t foundPCHDevice = 0;
	while (auto entry = OSDynamicCast(IORegistryEntry,
		iterator->getNextObject())) {
		uint32_t vendor = 0;
		uint32_t device = 0;
		if (!WIOKit::getOSDataValue(entry, "vendor-id", vendor) ||
			!WIOKit::getOSDataValue(entry, "device-id", device))
			continue;
		if (vendor == kTargetIntelVendorID &&
			device == kTargetCMLGT2DeviceID) {
			auto pci = OSDynamicCast(IOPCIDevice, entry);
			if (pci != nullptr) {
				foundIGPU = true;
				foundIGPUVendor = vendor;
				foundIGPUDevice = device;
				if (targetIGPUService == nullptr) {
					targetIGPUService = pci;
					targetIGPUService->retain();
				}
			}
		} else if (vendor == kTargetIntelVendorID &&
			device == kTargetCMP2PCHDeviceID) {
			foundPCH = true;
			foundPCHVendor = vendor;
			foundPCHDevice = device;
		}
		if (foundIGPU && foundPCH)
			break;
	}
	iterator->release();
	matching->release();

	const bool platformFound = foundIGPU && foundPCH &&
		targetIGPUService != nullptr;
	// BAR0 validation needs the platform gate, but the final target gate is
	// revoked again if the exact device-power boundary cannot be installed.
	targetCMLIGPUValidated = platformFound ? 1 : 0;
	const bool powerBoundaryReady = platformFound &&
		initIGPUPowerBoundary(targetIGPUService);
	if (targetIGPUService != nullptr)
		targetIGPUService->release();
	const bool found = platformFound && powerBoundaryReady;
	targetCMLIGPUValidated = found ? 1 : 0;
	publishRegister32("TargetIGPUVendorID", foundIGPUVendor);
	publishRegister32("TargetIGPUDeviceID", foundIGPUDevice);
	publishRegister32("TargetPCHVendorID", foundPCHVendor);
	publishRegister32("TargetPCHDeviceID", foundPCHDevice);
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty("TargetIGPUStatus",
			found ? "validated-intel-8086-9bc4-bar0-and-power-boundary" :
			(foundIGPU ? "refused-igpu-power-boundary-initialization-failed" :
			"refused-target-intel-8086-9bc4-not-found"));
		RazerOLEDWakeFix_selfInstance->setProperty("TargetPCHStatus",
			foundPCH ? "validated-intel-8086-068d-cmp2" :
			"refused-target-intel-8086-068d-cmp2-not-found");
	}
	SYSLOG("init", "target platform validation: IGPU=%u %04X:%04X CMP2-PCH=%u %04X:%04X BAR0/device-boundary=%u",
		foundIGPU, foundIGPUVendor, foundIGPUDevice,
		foundPCH, foundPCHVendor, foundPCHDevice, powerBoundaryReady);
	return found;
}

static void patchFramebuffer(void *, KernelPatcher &patcher, size_t index,
	mach_vm_address_t address, size_t size) {
	if (cflFramebuffer.loadIndex != index)
		return;
	if (!validateTargetCMLIGPU()) {
		publishPatchStatus("refused-non-target-igpu-or-cmp2-pch");
		SYSLOG("patch", "refused: v61 requires physical Intel 8086:9BC4 plus CMP2 PCH 8086:068D");
		return;
	}

	// Capture the physical panel-control pad while the firmware-lit OLED is
	// still known-good.  A mapping failure is diagnostic-only and never blocks
	// the already verified framebuffer routes.
	const bool pchGpioResult = initPchPanelGpioMapping();
	SYSLOG("gpio", "cold PCH panel GPIO mapping/capture result=%u", pchGpioResult);

	auto bytes = reinterpret_cast<uint8_t *>(address);
	size_t panelOriginalOffset = 0;
	size_t phase1ExitOriginalOffset = 0;
	size_t phase1FailureOriginalOffset = 0;
	size_t initialDDIBufferCallSiteOffset = 0;
	size_t ddiBufferCallSiteOffset = 0;
	size_t phase2DDIBufferCallSiteOffset = 0;
	size_t dpcdLaneSetCallSiteOffset = 0;
	size_t phase1ClockRecoveryDelayCallSiteOffset = 0;
	size_t nativeLinkConfigurationWriteCallSiteOffset = 0;
	size_t nativeEDPConfigurationWriteCallSiteOffset = 0;
	const bool panelOriginalIsUnique = findPatternOnce(findPanelSettleDelay,
		sizeof(findPanelSettleDelay), bytes, size, panelOriginalOffset);
	const bool phase1ExitOriginalIsUnique = findPatternOnce(findPhase1StableRetryExit,
		sizeof(findPhase1StableRetryExit), bytes, size, phase1ExitOriginalOffset);
	const bool phase1FailureOriginalIsUnique = findPatternOnce(findPhase1StableRetryFailure,
		sizeof(findPhase1StableRetryFailure), bytes, size, phase1FailureOriginalOffset);
	const bool initialDDIBufferCallSiteIsUnique = findPatternOnce(
		findInitialDDIBufferEnableCallSite,
		sizeof(findInitialDDIBufferEnableCallSite), bytes, size,
		initialDDIBufferCallSiteOffset);
	const bool ddiBufferCallSiteIsUnique = findPatternOnce(
		findDDIBufferEnableCallSite, sizeof(findDDIBufferEnableCallSite),
		bytes, size, ddiBufferCallSiteOffset);
	const bool phase2DDIBufferCallSiteIsUnique = findPatternOnce(
		findPhase2DDIBufferEnableCallSite,
		sizeof(findPhase2DDIBufferEnableCallSite), bytes, size,
		phase2DDIBufferCallSiteOffset);
	const bool dpcdLaneSetCallSiteIsUnique = findPatternOnce(
		findDPCDLaneSetWriteCallSite,
		sizeof(findDPCDLaneSetWriteCallSite), bytes, size,
		dpcdLaneSetCallSiteOffset);
	const bool phase1ClockRecoveryDelayCallSiteIsUnique = findPatternOnce(
		findPhase1ClockRecoveryDelayCallSite,
		sizeof(findPhase1ClockRecoveryDelayCallSite), bytes, size,
		phase1ClockRecoveryDelayCallSiteOffset);
	const bool nativeLinkConfigurationWriteCallSiteIsUnique = findPatternOnce(
		findNativeLinkConfigurationWriteCallSite,
		sizeof(findNativeLinkConfigurationWriteCallSite), bytes, size,
		nativeLinkConfigurationWriteCallSiteOffset);
	const bool nativeEDPConfigurationWriteCallSiteIsUnique = findPatternOnce(
		findNativeEDPConfigurationWriteCallSite,
		sizeof(findNativeEDPConfigurationWriteCallSite), bytes, size,
		nativeEDPConfigurationWriteCallSiteOffset);
	const bool nativeEDPConfigurationWriteCallShapeMatches =
		nativeEDPConfigurationWriteCallSiteIsUnique &&
		nativeEDPConfigurationWriteCallSiteOffset + 44 <= size &&
		bytes[nativeEDPConfigurationWriteCallSiteOffset + 35] == 0xE8 &&
		bytes[nativeEDPConfigurationWriteCallSiteOffset + 40] == 0x85 &&
		bytes[nativeEDPConfigurationWriteCallSiteOffset + 41] == 0xC0 &&
		bytes[nativeEDPConfigurationWriteCallSiteOffset + 42] == 0x74 &&
		bytes[nativeEDPConfigurationWriteCallSiteOffset + 43] == 0x28;
	const bool nativeLinkConfigurationWriteCallShapeMatches =
		nativeLinkConfigurationWriteCallSiteIsUnique &&
		nativeLinkConfigurationWriteCallSiteOffset + 66 <= size &&
		bytes[nativeLinkConfigurationWriteCallSiteOffset + 57] == 0xE8 &&
		bytes[nativeLinkConfigurationWriteCallSiteOffset + 62] == 0x85 &&
		bytes[nativeLinkConfigurationWriteCallSiteOffset + 63] == 0xC0 &&
		bytes[nativeLinkConfigurationWriteCallSiteOffset + 64] == 0x74 &&
		bytes[nativeLinkConfigurationWriteCallSiteOffset + 65] == 0x28;
	const bool phase1ClockRecoveryDelayCallShapeMatches =
		phase1ClockRecoveryDelayCallSiteIsUnique &&
		phase1ClockRecoveryDelayCallSiteOffset + 48 <= size &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 19] == 0xE8 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 24] == 0xEB &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 25] == 0x0B &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 26] == 0x8B &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 27] == 0xBD &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 28] == 0x30 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 29] == 0xFF &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 30] == 0xFF &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 31] == 0xFF &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 32] == 0xE8 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 37] == 0x4C &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 38] == 0x89 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 39] == 0xE7 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 40] == 0x4C &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 41] == 0x89 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 42] == 0xF6 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 43] == 0xBA &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 44] == 0x02 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 45] == 0x02 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 46] == 0x00 &&
		bytes[phase1ClockRecoveryDelayCallSiteOffset + 47] == 0x00;

	if (!panelOriginalIsUnique || !phase1ExitOriginalIsUnique ||
		!phase1FailureOriginalIsUnique ||
		!initialDDIBufferCallSiteIsUnique || !ddiBufferCallSiteIsUnique ||
		!phase2DDIBufferCallSiteIsUnique ||
		!dpcdLaneSetCallSiteIsUnique ||
		!nativeLinkConfigurationWriteCallShapeMatches ||
		!nativeEDPConfigurationWriteCallShapeMatches ||
		!phase1ClockRecoveryDelayCallShapeMatches) {
		publishPatchStatus("refused-preflight-pattern-missing-or-not-unique");
		SYSLOG("patch", "refused preflight: panel=%d phase1-exit=%d phase1-failure=%d initial-ddi-call=%d phase1-ddi-call=%d phase2-ddi-call=%d dpcd-lane-call=%d native-link-config-call=%d/shape=%d native-010a-call=%d/shape=%d phase1-delay-call=%d/shape=%d",
			panelOriginalIsUnique, phase1ExitOriginalIsUnique,
			phase1FailureOriginalIsUnique,
			initialDDIBufferCallSiteIsUnique, ddiBufferCallSiteIsUnique,
			phase2DDIBufferCallSiteIsUnique,
			dpcdLaneSetCallSiteIsUnique,
			nativeLinkConfigurationWriteCallSiteIsUnique,
			nativeLinkConfigurationWriteCallShapeMatches,
			nativeEDPConfigurationWriteCallSiteIsUnique,
			nativeEDPConfigurationWriteCallShapeMatches,
			phase1ClockRecoveryDelayCallSiteIsUnique,
			phase1ClockRecoveryDelayCallShapeMatches);
		return;
	}

	KernelPatcher::LookupPatch panelPatch {
		&cflFramebuffer,
		findPanelSettleDelay,
		replacePanelSettleDelay,
		sizeof(findPanelSettleDelay),
		1
	};
	KernelPatcher::LookupPatch phase1ExitPatch {
		&cflFramebuffer,
		findPhase1StableRetryExit,
		replacePhase1StableRetryExit,
		sizeof(findPhase1StableRetryExit),
		1
	};
	KernelPatcher::LookupPatch phase1FailurePatch {
		&cflFramebuffer,
		findPhase1StableRetryFailure,
		replacePhase1StableRetryFailure,
		sizeof(findPhase1StableRetryFailure),
		1
	};
	patcher.clearError();
	patcher.applyLookupPatch(&panelPatch, bytes, size);
	patcher.applyLookupPatch(&phase1ExitPatch, bytes, size);
	patcher.applyLookupPatch(&phase1FailurePatch, bytes, size);
	if (patcher.getError() != KernelPatcher::Error::NoError) {
		publishPatchStatus("failed-kernel-patcher-error");
		SYSLOG("patch", "failed with KernelPatcher error %d", patcher.getError());
		patcher.clearError();
		return;
	}

	size_t panelReplacementOffset = 0;
	size_t phase1ExitReplacementOffset = 0;
	size_t phase1FailureReplacementOffset = 0;
	size_t staleOriginalOffset = 0;
	const bool panelReplacementIsUnique = findPatternOnce(replacePanelSettleDelay,
		sizeof(replacePanelSettleDelay), bytes, size, panelReplacementOffset);
	const bool phase1ExitReplacementIsUnique = findPatternOnce(replacePhase1StableRetryExit,
		sizeof(replacePhase1StableRetryExit), bytes, size, phase1ExitReplacementOffset);
	const bool phase1FailureReplacementIsUnique = findPatternOnce(replacePhase1StableRetryFailure,
		sizeof(replacePhase1StableRetryFailure), bytes, size, phase1FailureReplacementOffset);
	const bool panelOriginalRemains = KernelPatcher::findPattern(findPanelSettleDelay, nullptr,
		sizeof(findPanelSettleDelay), bytes, size, &staleOriginalOffset);
	const bool phase1ExitOriginalRemains = KernelPatcher::findPattern(findPhase1StableRetryExit,
		nullptr, sizeof(findPhase1StableRetryExit), bytes, size, &staleOriginalOffset);
	const bool phase1FailureOriginalRemains = KernelPatcher::findPattern(findPhase1StableRetryFailure,
		nullptr, sizeof(findPhase1StableRetryFailure), bytes, size, &staleOriginalOffset);
	if (!panelReplacementIsUnique || !phase1ExitReplacementIsUnique ||
		!phase1FailureReplacementIsUnique ||
		panelOriginalRemains ||
		phase1ExitOriginalRemains || phase1FailureOriginalRemains ||
		panelReplacementOffset != panelOriginalOffset ||
		phase1ExitReplacementOffset != phase1ExitOriginalOffset ||
		phase1FailureReplacementOffset != phase1FailureOriginalOffset) {
		publishPatchStatus("failed-post-write-verification");
		SYSLOG("patch", "post-write verification failed");
		return;
	}

	patcher.clearError();
	callSetDPPowerState = patcher.solveSymbol<SetDPPowerState>(index,
		setDPPowerStateSymbol, address, size);
	callReadAUX = patcher.solveSymbol<ReadAUX>(index,
		readAUXSymbol, address, size);
	callWriteAUX = patcher.solveSymbol<WriteAUX>(index,
		writeAUXSymbol, address, size);
	callReadRegister32 = patcher.solveSymbol<ReadRegister32>(index,
		readRegister32Symbol, address, size);
	callWriteRegister32 = patcher.solveSymbol<WriteRegister32>(index,
		writeRegister32Symbol, address, size);
	if (callSetDPPowerState == nullptr || callReadAUX == nullptr ||
		callWriteAUX == nullptr ||
		callReadRegister32 == nullptr ||
		callWriteRegister32 == nullptr ||
		patcher.getError() != KernelPatcher::Error::NoError) {
		publishPatchStatus("failed-dp-power-aux-or-register-symbol");
		SYSLOG("patch", "failed to solve SetDPPowerState/ReadAUX/WriteAUX/ReadRegister32/WriteRegister32");
		patcher.clearError();
		return;
	}
	if (RazerOLEDWakeFix_selfInstance != nullptr) {
		RazerOLEDWakeFix_selfInstance->setProperty(
			"HPDRegisterSnapshotLayout",
			"u32[6]=44030,C4000,C4004,C400C,C4030,C4038");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"WindowsReferenceFunction",
			"igdkmd64-31.0.101.2141-SetLinkConfiguration-0115-rate-selector-then-0101-lanes");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"LinuxReferenceFunction",
			"i915-intel_dp_link_training_set_bw-supported-link-rates-0115");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"TahoeNativeLinkConfigurationEvidence",
			"Tahoe25.2-LinkTraining+0x413-DPCD0100-native-14-84-call-site");
		RazerOLEDWakeFix_selfInstance->setProperty(
			"TahoeNativeOverwriteEvidence",
			"Tahoe25.2-LinkTraining+0x4A4-DPCD010A-native-zero-overwrite-captured-v53");
	}

	KernelPatcher::RouteRequest routes[] {
		KernelPatcher::RouteRequest(hwSetPanelPowerSymbol,
			wrapHwSetPanelPower, originalHwSetPanelPower),
		KernelPatcher::RouteRequest(hwSetPanelPowerConfigSymbol,
			wrapHwSetPanelPowerConfig, originalHwSetPanelPowerConfig),
		KernelPatcher::RouteRequest(enableClocksSymbol,
			wrapEnableClocks, originalEnableClocks),
		KernelPatcher::RouteRequest(lightUpEDPSymbol,
			wrapLightUpEDP, originalLightUpEDP),
		KernelPatcher::RouteRequest(configureBufferTranslationSymbol,
			wrapConfigureBufferTranslation, originalConfigureBufferTranslation),
		KernelPatcher::RouteRequest(checkClockRecoverySymbol,
			wrapCheckClockRecovery, originalCheckClockRecovery),
		KernelPatcher::RouteRequest(setupParamsSymbol,
			wrapSetupParams, originalSetupParams),
		KernelPatcher::RouteRequest(setPowerWellStateSymbol,
			wrapSetPowerWellState, originalSetPowerWellState),
		KernelPatcher::RouteRequest(framebufferDoSetPowerStateSymbol,
			wrapFramebufferDoSetPowerState,
			originalFramebufferDoSetPowerState)
	};
	patcher.clearError();
	if (!patcher.routeMultipleLong(index, routes, arrsize(routes), address, size) ||
		originalHwSetPanelPower == nullptr || originalHwSetPanelPowerConfig == nullptr ||
		originalEnableClocks == nullptr ||
		originalLightUpEDP == nullptr ||
		originalConfigureBufferTranslation == nullptr ||
		originalCheckClockRecovery == nullptr ||
		originalSetupParams == nullptr ||
		originalSetPowerWellState == nullptr ||
		originalFramebufferDoSetPowerState == nullptr) {
		publishPatchStatus("failed-panel-clocks-training-powerwell-or-fb-power-routes");
		SYSLOG("patch", "failed to route panel power/config, EnableClocks, LightUpEDP, buffer translation, SetupParams, clock-recovery diagnostic, display power-well, or FB0 power-state callbacks");
		patcher.clearError();
		return;
	}

	// Retarget Tahoe's one initial native LinkTraining WriteAUX(0x100, 2)
	// call.  The replacement performs the eDP 1.4 rate-selector sequence only
	// on the exact S3/internal-panel call and otherwise forwards it unchanged.
	const auto nativeLinkConfigurationWriteCallAddress = address +
		nativeLinkConfigurationWriteCallSiteOffset +
		kNativeLinkConfigurationWriteCallOffset;
	const auto nativeLinkConfigurationWriteCallNext =
		nativeLinkConfigurationWriteCallAddress + kRelativeCallSize;
	const auto nativeLinkConfigurationWriteCallTarget =
		reinterpret_cast<mach_vm_address_t>(
			writeEDP14RateSelectAtNativeLinkTrainingCall);
	const auto nativeLinkConfigurationWriteCallDelta =
		static_cast<int64_t>(nativeLinkConfigurationWriteCallTarget) -
		static_cast<int64_t>(nativeLinkConfigurationWriteCallNext);
	if (nativeLinkConfigurationWriteCallDelta < -2147483648LL ||
		nativeLinkConfigurationWriteCallDelta > 2147483647LL) {
		publishPatchStatus("failed-native-link-config-call-target-out-of-range");
		SYSLOG("patch", "native LinkTraining link-configuration call target is outside rel32 range");
		return;
	}
	const auto nativeLinkConfigurationWriteRelativeCall =
		static_cast<uint32_t>(
			static_cast<int32_t>(nativeLinkConfigurationWriteCallDelta));
	uint8_t nativeLinkConfigurationWriteCallPatch[kRelativeCallSize] {
		0xE8,
		static_cast<uint8_t>(
			nativeLinkConfigurationWriteRelativeCall & 0xFF),
		static_cast<uint8_t>(
			(nativeLinkConfigurationWriteRelativeCall >> 8) & 0xFF),
		static_cast<uint8_t>(
			(nativeLinkConfigurationWriteRelativeCall >> 16) & 0xFF),
		static_cast<uint8_t>(
			(nativeLinkConfigurationWriteRelativeCall >> 24) & 0xFF)
	};
	patcher.clearError();
	if (patcher.routeBlock(nativeLinkConfigurationWriteCallAddress,
		nativeLinkConfigurationWriteCallPatch,
		sizeof(nativeLinkConfigurationWriteCallPatch)) != 0 ||
		patcher.getError() != KernelPatcher::Error::NoError) {
		publishPatchStatus("failed-native-link-config-call-site-route");
		SYSLOG("patch", "failed to route native LinkTraining link-configuration WriteAUX call");
		patcher.clearError();
		return;
	}
	for (size_t byte = 0;
		byte < sizeof(nativeLinkConfigurationWriteCallPatch); byte++) {
		if (*(reinterpret_cast<const uint8_t *>(
			nativeLinkConfigurationWriteCallAddress) + byte) !=
			nativeLinkConfigurationWriteCallPatch[byte]) {
			publishPatchStatus(
				"failed-native-link-config-call-site-verification");
			SYSLOG("patch", "native LinkTraining link-configuration call verification failed at byte %lu",
				byte);
			return;
		}
	}

	// Retarget Tahoe's one native LinkTraining DPCD 0x10A WriteAUX call.  The
	// wrapper preserves the original function ABI and substitutes 0x01 only
	// when the live call is the exact 0x10A/length-1/value-0 operation on the
	// validated internal eDP path during a committed S3 wake.
	const auto nativeEDPConfigurationWriteCallAddress = address +
		nativeEDPConfigurationWriteCallSiteOffset +
		kNativeEDPConfigurationWriteCallOffset;
	const auto nativeEDPConfigurationWriteCallNext =
		nativeEDPConfigurationWriteCallAddress + kRelativeCallSize;
	const auto nativeEDPConfigurationWriteCallTarget =
		reinterpret_cast<mach_vm_address_t>(
			writeWindowsEDPConfigurationAtNativeLinkTrainingCall);
	const auto nativeEDPConfigurationWriteCallDelta =
		static_cast<int64_t>(nativeEDPConfigurationWriteCallTarget) -
		static_cast<int64_t>(nativeEDPConfigurationWriteCallNext);
	if (nativeEDPConfigurationWriteCallDelta < -2147483648LL ||
		nativeEDPConfigurationWriteCallDelta > 2147483647LL) {
		publishPatchStatus("failed-native-010a-call-target-out-of-range");
		SYSLOG("patch", "native LinkTraining DPCD 0x10A call target is outside rel32 range");
		return;
	}
	const auto nativeEDPConfigurationWriteRelativeCall =
		static_cast<uint32_t>(
			static_cast<int32_t>(nativeEDPConfigurationWriteCallDelta));
	uint8_t nativeEDPConfigurationWriteCallPatch[kRelativeCallSize] {
		0xE8,
		static_cast<uint8_t>(
			nativeEDPConfigurationWriteRelativeCall & 0xFF),
		static_cast<uint8_t>(
			(nativeEDPConfigurationWriteRelativeCall >> 8) & 0xFF),
		static_cast<uint8_t>(
			(nativeEDPConfigurationWriteRelativeCall >> 16) & 0xFF),
		static_cast<uint8_t>(
			(nativeEDPConfigurationWriteRelativeCall >> 24) & 0xFF)
	};
	patcher.clearError();
	if (patcher.routeBlock(nativeEDPConfigurationWriteCallAddress,
		nativeEDPConfigurationWriteCallPatch,
		sizeof(nativeEDPConfigurationWriteCallPatch)) != 0 ||
		patcher.getError() != KernelPatcher::Error::NoError) {
		publishPatchStatus("failed-native-010a-call-site-route");
		SYSLOG("patch", "failed to route native LinkTraining DPCD 0x10A WriteAUX call");
		patcher.clearError();
		return;
	}
	for (size_t byte = 0;
		byte < sizeof(nativeEDPConfigurationWriteCallPatch); byte++) {
		if (*(reinterpret_cast<const uint8_t *>(
			nativeEDPConfigurationWriteCallAddress) + byte) !=
			nativeEDPConfigurationWriteCallPatch[byte]) {
			publishPatchStatus(
				"failed-native-010a-call-site-verification");
			SYSLOG("patch", "native LinkTraining DPCD 0x10A call verification failed at byte %lu",
				byte);
			return;
		}
	}

	// Retarget only Phase 1's IOSleep immediately before Tahoe reads DPCD
	// 0x202.  The source prefix and both sides of the relocated call were
	// verified during preflight, so Phase 2's separate wait remains native.
	const auto phase1ClockRecoveryDelayCallAddress = address +
		phase1ClockRecoveryDelayCallSiteOffset +
		kPhase1ClockRecoveryDelayCallOffset;
	const auto phase1ClockRecoveryDelayCallNext =
		phase1ClockRecoveryDelayCallAddress + kRelativeCallSize;
	const auto phase1ClockRecoveryDelayCallTarget =
		reinterpret_cast<mach_vm_address_t>(
			sleepPhase1ClockRecoveryWindowsInterval);
	const auto phase1ClockRecoveryDelayCallDelta =
		static_cast<int64_t>(phase1ClockRecoveryDelayCallTarget) -
		static_cast<int64_t>(phase1ClockRecoveryDelayCallNext);
	if (phase1ClockRecoveryDelayCallDelta < -2147483648LL ||
		phase1ClockRecoveryDelayCallDelta > 2147483647LL) {
		publishPatchStatus("failed-phase1-delay-call-target-out-of-range");
		SYSLOG("patch", "Phase-1 Windows interval call target is outside rel32 range");
		return;
	}
	const auto phase1ClockRecoveryDelayRelativeCall = static_cast<uint32_t>(
		static_cast<int32_t>(phase1ClockRecoveryDelayCallDelta));
	uint8_t phase1ClockRecoveryDelayCallPatch[kRelativeCallSize] {
		0xE8,
		static_cast<uint8_t>(
			phase1ClockRecoveryDelayRelativeCall & 0xFF),
		static_cast<uint8_t>(
			(phase1ClockRecoveryDelayRelativeCall >> 8) & 0xFF),
		static_cast<uint8_t>(
			(phase1ClockRecoveryDelayRelativeCall >> 16) & 0xFF),
		static_cast<uint8_t>(
			(phase1ClockRecoveryDelayRelativeCall >> 24) & 0xFF)
	};
	patcher.clearError();
	if (patcher.routeBlock(phase1ClockRecoveryDelayCallAddress,
		phase1ClockRecoveryDelayCallPatch,
		sizeof(phase1ClockRecoveryDelayCallPatch)) != 0 ||
		patcher.getError() != KernelPatcher::Error::NoError) {
		publishPatchStatus("failed-phase1-delay-call-site-route");
		SYSLOG("patch", "failed to route LinkTraining Phase-1 Windows interval call");
		patcher.clearError();
		return;
	}
	for (size_t byte = 0;
		byte < sizeof(phase1ClockRecoveryDelayCallPatch); byte++) {
		if (*(reinterpret_cast<const uint8_t *>(
			phase1ClockRecoveryDelayCallAddress) + byte) !=
			phase1ClockRecoveryDelayCallPatch[byte]) {
			publishPatchStatus(
				"failed-phase1-delay-call-site-verification");
			SYSLOG("patch", "LinkTraining Phase-1 delay call verification failed at byte %lu",
				byte);
			return;
		}
	}

	// Retarget the first LinkTraining DDI_BUF_CTL_A enable.  This call returns
	// immediately before Tahoe writes sink DPCD 0x102=Pattern 1, so the wrapper
	// can verify/repair the source Pattern-1 state and wait for carrier startup
	// at the only ordering point that V47 did not instrument.
	const auto initialDDIBufferCallAddress = address +
		initialDDIBufferCallSiteOffset + kInitialDDIBufferEnableCallOffset;
	const auto initialDDIBufferCallNext = initialDDIBufferCallAddress +
		kRelativeCallSize;
	const auto initialDDIBufferCallTarget =
		reinterpret_cast<mach_vm_address_t>(
			writeInitialDDIBufferControlAndVerify);
	const auto initialDDIBufferCallDelta =
		static_cast<int64_t>(initialDDIBufferCallTarget) -
		static_cast<int64_t>(initialDDIBufferCallNext);
	if (initialDDIBufferCallDelta < -2147483648LL ||
		initialDDIBufferCallDelta > 2147483647LL) {
		publishPatchStatus("failed-initial-ddi-call-target-out-of-range");
		SYSLOG("patch", "initial DDI carrier call target is outside rel32 range");
		return;
	}
	const auto initialDDIRelativeCall = static_cast<uint32_t>(
		static_cast<int32_t>(initialDDIBufferCallDelta));
	uint8_t initialDDIBufferCallPatch[kRelativeCallSize] {
		0xE8,
		static_cast<uint8_t>(initialDDIRelativeCall & 0xFF),
		static_cast<uint8_t>((initialDDIRelativeCall >> 8) & 0xFF),
		static_cast<uint8_t>((initialDDIRelativeCall >> 16) & 0xFF),
		static_cast<uint8_t>((initialDDIRelativeCall >> 24) & 0xFF)
	};
	patcher.clearError();
	if (patcher.routeBlock(initialDDIBufferCallAddress,
		initialDDIBufferCallPatch, sizeof(initialDDIBufferCallPatch)) != 0 ||
		patcher.getError() != KernelPatcher::Error::NoError) {
		publishPatchStatus("failed-initial-ddi-call-site-route");
		SYSLOG("patch", "failed to route initial LinkTraining DDI carrier call");
		patcher.clearError();
		return;
	}
	for (size_t byte = 0; byte < sizeof(initialDDIBufferCallPatch); byte++) {
		if (*(reinterpret_cast<const uint8_t *>(
			initialDDIBufferCallAddress) + byte) !=
			initialDDIBufferCallPatch[byte]) {
			publishPatchStatus(
				"failed-initial-ddi-call-site-verification");
			SYSLOG("patch", "initial LinkTraining DDI carrier call verification failed at byte %lu",
				byte);
			return;
		}
	}

	// Retarget only LinkTraining's DDI_BUF_CTL_A enable call to the diagnostic
	// pass-through.  It preserves Apple's value and adds only the V52 1 ms
	// settling observation on a committed S3 wake.
	const auto ddiBufferCallAddress = address + ddiBufferCallSiteOffset +
		kDDIBufferEnableCallOffset;
	const auto ddiBufferCallNext = ddiBufferCallAddress + kRelativeCallSize;
	const auto ddiBufferCallTarget = reinterpret_cast<mach_vm_address_t>(
		writeDDIBufferControlAndSettle);
	const auto ddiBufferCallDelta = static_cast<int64_t>(ddiBufferCallTarget) -
		static_cast<int64_t>(ddiBufferCallNext);
	if (ddiBufferCallDelta < -2147483648LL ||
		ddiBufferCallDelta > 2147483647LL) {
		publishPatchStatus("failed-ddi-buffer-call-target-out-of-range");
		SYSLOG("patch", "DDI buffer settle call target is outside rel32 range");
		return;
	}
	const auto relativeCall = static_cast<uint32_t>(
		static_cast<int32_t>(ddiBufferCallDelta));
	uint8_t ddiBufferCallPatch[kRelativeCallSize] {
		0xE8,
		static_cast<uint8_t>(relativeCall & 0xFF),
		static_cast<uint8_t>((relativeCall >> 8) & 0xFF),
		static_cast<uint8_t>((relativeCall >> 16) & 0xFF),
		static_cast<uint8_t>((relativeCall >> 24) & 0xFF)
	};
	patcher.clearError();
	if (patcher.routeBlock(ddiBufferCallAddress, ddiBufferCallPatch,
		sizeof(ddiBufferCallPatch)) != 0 ||
		patcher.getError() != KernelPatcher::Error::NoError) {
		publishPatchStatus("failed-ddi-buffer-call-site-route");
		SYSLOG("patch", "failed to route the unique LinkTraining DDI source diagnostic call");
		patcher.clearError();
		return;
	}
	for (size_t byte = 0; byte < sizeof(ddiBufferCallPatch); byte++) {
		if (*(reinterpret_cast<const uint8_t *>(ddiBufferCallAddress) + byte) !=
			ddiBufferCallPatch[byte]) {
			publishPatchStatus("failed-ddi-buffer-call-site-verification");
			SYSLOG("patch", "LinkTraining DDI buffer enable call verification failed at byte %lu",
				byte);
			return;
		}
	}

	// Phase 2 has a distinct source-strength call.  Route it only after the
	// exact Tahoe 25.2 sequence was proven unique, and verify every rel32 byte
	// before the S3 state machine is armed.
	const auto phase2DDIBufferCallAddress = address +
		phase2DDIBufferCallSiteOffset + kPhase2DDIBufferEnableCallOffset;
	const auto phase2DDIBufferCallNext =
		phase2DDIBufferCallAddress + kRelativeCallSize;
	const auto phase2DDIBufferCallTarget =
		reinterpret_cast<mach_vm_address_t>(
			writePhase2DDIBufferControlWithLinuxKBLIBoost);
	const auto phase2DDIBufferCallDelta =
		static_cast<int64_t>(phase2DDIBufferCallTarget) -
		static_cast<int64_t>(phase2DDIBufferCallNext);
	if (phase2DDIBufferCallDelta < -2147483648LL ||
		phase2DDIBufferCallDelta > 2147483647LL) {
		publishPatchStatus("failed-phase2-ddi-call-target-out-of-range");
		SYSLOG("patch", "Phase-2 DDI I_boost call target is outside rel32 range");
		return;
	}
	const auto phase2DDIRelativeCall = static_cast<uint32_t>(
		static_cast<int32_t>(phase2DDIBufferCallDelta));
	uint8_t phase2DDIBufferCallPatch[kRelativeCallSize] {
		0xE8,
		static_cast<uint8_t>(phase2DDIRelativeCall & 0xFF),
		static_cast<uint8_t>((phase2DDIRelativeCall >> 8) & 0xFF),
		static_cast<uint8_t>((phase2DDIRelativeCall >> 16) & 0xFF),
		static_cast<uint8_t>((phase2DDIRelativeCall >> 24) & 0xFF)
	};
	patcher.clearError();
	if (patcher.routeBlock(phase2DDIBufferCallAddress,
		phase2DDIBufferCallPatch, sizeof(phase2DDIBufferCallPatch)) != 0 ||
		patcher.getError() != KernelPatcher::Error::NoError) {
		publishPatchStatus("failed-phase2-ddi-call-site-route");
		SYSLOG("patch", "failed to route the unique LinkTraining Phase-2 DDI I_boost call");
		patcher.clearError();
		return;
	}
	for (size_t byte = 0; byte < sizeof(phase2DDIBufferCallPatch); byte++) {
		if (*(reinterpret_cast<const uint8_t *>(
			phase2DDIBufferCallAddress) + byte) !=
			phase2DDIBufferCallPatch[byte]) {
			publishPatchStatus(
				"failed-phase2-ddi-call-site-verification");
			SYSLOG("patch", "LinkTraining Phase-2 DDI I_boost call verification failed at byte %lu",
				byte);
			return;
		}
	}

	// Retarget only the paired DPCD 0x103 lane-set WriteAUX call in the same
	// Phase-1 adjustment loop to an ABI-identical diagnostic pass-through.
	// V57 preserves V56's byte-for-byte forwarding and invokes WriteAUX once.
	const auto dpcdLaneSetCallAddress = address + dpcdLaneSetCallSiteOffset +
		kDPCDLaneSetWriteCallOffset;
	const auto dpcdLaneSetCallNext = dpcdLaneSetCallAddress +
		kRelativeCallSize;
	const auto dpcdLaneSetCallTarget = reinterpret_cast<mach_vm_address_t>(
		writeNativePhase1LaneSetForS3);
	const auto dpcdLaneSetCallDelta =
		static_cast<int64_t>(dpcdLaneSetCallTarget) -
		static_cast<int64_t>(dpcdLaneSetCallNext);
	if (dpcdLaneSetCallDelta < -2147483648LL ||
		dpcdLaneSetCallDelta > 2147483647LL) {
		publishPatchStatus("failed-dpcd-lane-call-target-out-of-range");
		SYSLOG("patch", "DPCD lane-set call target is outside rel32 range");
		return;
	}
	const auto dpcdRelativeCall = static_cast<uint32_t>(
		static_cast<int32_t>(dpcdLaneSetCallDelta));
	uint8_t dpcdLaneSetCallPatch[kRelativeCallSize] {
		0xE8,
		static_cast<uint8_t>(dpcdRelativeCall & 0xFF),
		static_cast<uint8_t>((dpcdRelativeCall >> 8) & 0xFF),
		static_cast<uint8_t>((dpcdRelativeCall >> 16) & 0xFF),
		static_cast<uint8_t>((dpcdRelativeCall >> 24) & 0xFF)
	};
	patcher.clearError();
	if (patcher.routeBlock(dpcdLaneSetCallAddress, dpcdLaneSetCallPatch,
		sizeof(dpcdLaneSetCallPatch)) != 0 ||
		patcher.getError() != KernelPatcher::Error::NoError) {
		publishPatchStatus("failed-dpcd-lane-call-site-route");
		SYSLOG("patch", "failed to route the unique Phase-1 DPCD sink diagnostic call");
		patcher.clearError();
		return;
	}
	for (size_t byte = 0; byte < sizeof(dpcdLaneSetCallPatch); byte++) {
		if (*(reinterpret_cast<const uint8_t *>(dpcdLaneSetCallAddress) + byte) !=
			dpcdLaneSetCallPatch[byte]) {
			publishPatchStatus("failed-dpcd-lane-call-site-verification");
			SYSLOG("patch", "LinkTraining DPCD lane-set call verification failed at byte %lu",
				byte);
			return;
		}
	}

	// Publish the device-boundary state machine only after every symbol route and
	// every call-site patch has been verified.  Earlier PM callbacks fail closed.
	if (southChickenLock != nullptr)
		IOLockLock(southChickenLock);
	OSSynchronizeIO();
	v61PatchFullyArmed = 1;
	OSSynchronizeIO();
	if (southChickenLock != nullptr)
		IOLockUnlock(southChickenLock);
	if (RazerOLEDWakeFix_selfInstance != nullptr)
		RazerOLEDWakeFix_selfInstance->setProperty("V61PatchArmStatus",
			"all-v60-routes-plus-edp14-rate-selector-call-site-verified");
	publishPatchStatus("applied-verified-s3-edp14-rate-select-v61");
	SYSLOG("patch", "applied and verified v61: preserve all validated V60 CML/CMP2, dual-notifier and Linux KBL dynamic I_boost logic; replace only Tahoe 25.2's initial native LinkTraining 0x100/2-byte write with the Windows/Linux eDP 1.4 0x115 selector-2 plus 0x101/x4 sequence after exact live table validation, with native fallback on every mismatch; native-link-config call=0x%lX native-010a call=0x%lX phase1-delay call=0x%lX native/applied=%u/%ums initial call=0x%lX phase1-source=0x%lX phase2-source=0x%lX phase1-sink=0x%lX; DDI settle=%uus; panel=0x%lX; Phase 1 retries=0x%lX/0x%lX",
		nativeLinkConfigurationWriteCallSiteOffset +
			kNativeLinkConfigurationWriteCallOffset,
		nativeEDPConfigurationWriteCallSiteOffset +
			kNativeEDPConfigurationWriteCallOffset,
		phase1ClockRecoveryDelayCallSiteOffset +
			kPhase1ClockRecoveryDelayCallOffset,
		kApplePhase1ClockRecoveryDelayMs,
		kWindowsPhase1ClockRecoveryDelayMs,
		initialDDIBufferCallSiteOffset +
			kInitialDDIBufferEnableCallOffset,
		ddiBufferCallSiteOffset + kDDIBufferEnableCallOffset,
		phase2DDIBufferCallSiteOffset + kPhase2DDIBufferEnableCallOffset,
		dpcdLaneSetCallSiteOffset + kDPCDLaneSetWriteCallOffset,
		kDDIBufferActivationSettleUs, panelReplacementOffset,
		phase1ExitReplacementOffset, phase1FailureReplacementOffset);
}

static void initRazerOLEDWakeFix() {
	if (getKernelVersion() != KernelVersion::Tahoe || getKernelMinorVersion() != 2) {
		publishPatchStatus("refused-unsupported-darwin");
		SYSLOG("init", "refused: unsupported Darwin version %d.%d",
			getKernelVersion(), getKernelMinorVersion());
		return;
	}

	priorityPowerNotifier = registerPrioritySleepWakeInterest(
		priorityTerminationInterest, nullptr, nullptr);
	if (priorityPowerNotifier == nullptr) {
		publishPatchStatus("failed-priority-system-power-interest");
		SYSLOG("init", "failed to register priority shutdown/restart interest; refusing unsafe OLED power routing");
		return;
	}

	sleepWakeNotifier = registerSleepWakeInterest(
		systemPowerInterest, nullptr, nullptr);
	if (sleepWakeNotifier == nullptr) {
		priorityPowerNotifier->remove();
		priorityPowerNotifier = nullptr;
		publishPatchStatus("failed-early-system-sleep-wake-interest");
		SYSLOG("init", "failed to register early sleep/wake interest; refusing late-only OLED power routing");
		return;
	}

	publishPatchStatus("armed-waiting-for-cfl-framebuffer-with-dual-power-notifiers");
	SYSLOG("init", "v61 armed for AppleIntelCFLGraphicsFramebuffer on Darwin 25.2 with bounded eDP 1.4 rate-select substitution and dual power notifications");
	lilu.onKextLoadForce(&cflFramebuffer, 1, patchFramebuffer, nullptr);
}

static const char *disableArguments[] {
	"-razeroledoff"
};

PluginConfiguration ADDPR(config) {
	xStringify(PRODUCT_NAME),
	parseModuleVersion(xStringify(MODULE_VERSION)),
	LiluAPI::AllowNormal,
	disableArguments,
	arrsize(disableArguments),
	nullptr,
	0,
	nullptr,
	0,
	KernelVersion::Tahoe,
	KernelVersion::Tahoe,
	initRazerOLEDWakeFix
};

// MIT. Shared by kernel candidate and sanitizer-backed host tests.
#pragma once
#include "Legacy25C56Patterns.hpp"
#include "Patterns25G241.hpp"

namespace RazerOLEDPatchPlan {
enum class Target : uint8_t { None, Register32, AUX, Sleep };
struct ByteCheck { size_t offset; uint8_t value; };
struct Rule {
    const uint8_t *find;
    const uint8_t *replace;
    size_t length, callOffset;
    Target target;
    const ByteCheck *suffix;
    size_t suffixCount;
};
struct Layout {
    size_t panelOn, panelOff, crtcDdi, framebufferIndex, framebufferPower, pathType, pathPort;
};
static constexpr Layout legacyLayout {0x2C20,0x2C24,0x44,0x1DC,0xA1F8,0x28,0x450};
static constexpr Layout newLayout {0x2AA8,0x2AAC,0x44,0x1DC,0xA1F8,0x28,0x450};
template <typename T, size_t N> constexpr size_t count(const T (&)[N]) { return N; }
static const ByteCheck legacyDelay[] {
    {19,0xE8},{24,0xEB},{25,0x0B},{26,0x8B},{27,0xBD},{28,0x30},
    {29,0xFF},{30,0xFF},{31,0xFF},{32,0xE8},{37,0x4C},{38,0x89},
    {39,0xE7},{40,0x4C},{41,0x89},{42,0xF6},{43,0xBA},{44,0x02},
    {45,0x02},{46,0x00},{47,0x00}
};
static const ByteCheck legacyLink[] {{57,0xE8},{62,0x85},{63,0xC0},{64,0x74},{65,0x28}};
static const ByteCheck legacyEdp[] {{35,0xE8},{40,0x85},{41,0xC0},{42,0x74},{43,0x28}};
static const ByteCheck newDelay[] {
    {29,0xE8},{34,0xEB},{35,0x0B},{36,0x8B},{37,0xBD},{38,0x2C},
    {39,0xFF},{40,0xFF},{41,0xFF},{42,0xE8},{47,0x48},{48,0x89},
    {49,0xDF},{50,0x48},{51,0x8B},{52,0x75},{53,0xB0},{54,0xBA},
    {55,0x02},{56,0x02},{57,0x00},{58,0x00}
};
static const ByteCheck newLink[] {{57,0xE8},{62,0x85},{63,0xC0},{64,0x74},{65,0x2F}};
static const ByteCheck newEdp[] {{43,0xE8},{48,0x85},{49,0xC0},{50,0x74},{51,0x2F}};
static const Rule legacyRules[] {
    {findPanelSettleDelay, replacePanelSettleDelay,
        sizeof(findPanelSettleDelay), 0, Target::None,
        nullptr, 0},
    {findPhase1StableRetryExit, replacePhase1StableRetryExit,
        sizeof(findPhase1StableRetryExit), 0, Target::None,
        nullptr, 0},
    {findPhase1StableRetryFailure, replacePhase1StableRetryFailure,
        sizeof(findPhase1StableRetryFailure), 0, Target::None,
        nullptr, 0},
    {findInitialDDIBufferEnableCallSite, nullptr,
        sizeof(findInitialDDIBufferEnableCallSite), 30, Target::Register32,
        nullptr, 0},
    {findDDIBufferEnableCallSite, nullptr,
        sizeof(findDDIBufferEnableCallSite), 21, Target::Register32,
        nullptr, 0},
    {findPhase2DDIBufferEnableCallSite, nullptr,
        sizeof(findPhase2DDIBufferEnableCallSite), 38, Target::Register32,
        nullptr, 0},
    {findDPCDLaneSetWriteCallSite, nullptr,
        sizeof(findDPCDLaneSetWriteCallSite), 27, Target::AUX,
        nullptr, 0},
    {findPhase1ClockRecoveryDelayCallSite, nullptr,
        sizeof(findPhase1ClockRecoveryDelayCallSite), 32, Target::Sleep,
        legacyDelay, count(legacyDelay)},
    {findNativeLinkConfigurationWriteCallSite, nullptr,
        sizeof(findNativeLinkConfigurationWriteCallSite), 57, Target::AUX,
        legacyLink, count(legacyLink)},
    {findNativeEDPConfigurationWriteCallSite, nullptr,
        sizeof(findNativeEDPConfigurationWriteCallSite), 35, Target::AUX,
        legacyEdp, count(legacyEdp)},
};
static const Rule newRules[] {
    {RazerOLED25G241::findPanelSettleDelay, RazerOLED25G241::replacePanelSettleDelay,
        sizeof(RazerOLED25G241::findPanelSettleDelay), 0, Target::None,
        nullptr, 0},
    {RazerOLED25G241::findPhase1StableRetryExit, RazerOLED25G241::replacePhase1StableRetryExit,
        sizeof(RazerOLED25G241::findPhase1StableRetryExit), 0, Target::None,
        nullptr, 0},
    {RazerOLED25G241::findPhase1StableRetryFailure, RazerOLED25G241::replacePhase1StableRetryFailure,
        sizeof(RazerOLED25G241::findPhase1StableRetryFailure), 0, Target::None,
        nullptr, 0},
    {RazerOLED25G241::findInitialDDIBufferEnableCallSite, nullptr,
        sizeof(RazerOLED25G241::findInitialDDIBufferEnableCallSite), 28, Target::Register32,
        nullptr, 0},
    {RazerOLED25G241::findDDIBufferEnableCallSite, nullptr,
        sizeof(RazerOLED25G241::findDDIBufferEnableCallSite), 33, Target::Register32,
        nullptr, 0},
    {RazerOLED25G241::findPhase2DDIBufferEnableCallSite, nullptr,
        sizeof(RazerOLED25G241::findPhase2DDIBufferEnableCallSite), 35, Target::Register32,
        nullptr, 0},
    {RazerOLED25G241::findDPCDLaneSetWriteCallSite, nullptr,
        sizeof(RazerOLED25G241::findDPCDLaneSetWriteCallSite), 31, Target::AUX,
        nullptr, 0},
    {RazerOLED25G241::findPhase1ClockRecoveryDelayCallSite, nullptr,
        sizeof(RazerOLED25G241::findPhase1ClockRecoveryDelayCallSite), 42, Target::Sleep,
        newDelay, count(newDelay)},
    {RazerOLED25G241::findNativeLinkConfigurationWriteCallSite, nullptr,
        sizeof(RazerOLED25G241::findNativeLinkConfigurationWriteCallSite), 57, Target::AUX,
        newLink, count(newLink)},
    {RazerOLED25G241::findNativeEDPConfigurationWriteCallSite, nullptr,
        sizeof(RazerOLED25G241::findNativeEDPConfigurationWriteCallSite), 43, Target::AUX,
        newEdp, count(newEdp)},
};
struct Set { const Rule *rules; const Layout *layout; };
static constexpr Set legacySet {legacyRules,&legacyLayout};
static constexpr Set newSet {newRules,&newLayout};
static constexpr size_t ruleCount = 10;

// Read-only, bounded preflight. Reject missing/duplicate/corrupt/overlapping sites.
static inline bool preflight(const Set &set, const uint8_t *bytes, size_t length,
    size_t (&offsets)[ruleCount]) {
    if (bytes == nullptr || set.rules == nullptr) return false;
    for (size_t r=0;r<ruleCount;++r) {
        const auto &rule=set.rules[r];
        if (rule.length == 0 || rule.length > length) return false;
        size_t matches=0, found=0;
        for (size_t p=0;p<=length-rule.length;++p) {
            bool same=true;
            for (size_t i=0;i<rule.length && same;++i) same=bytes[p+i]==rule.find[i];
            if (same) { found=p; if (++matches>1) return false; }
        }
        if (matches != 1) return false;
        for (size_t i=0;i<rule.suffixCount;++i) {
            const auto &check=rule.suffix[i];
            if (check.offset >= length-found || bytes[found+check.offset] != check.value)
                return false;
        }
        if (rule.target != Target::None &&
            (rule.callOffset > length-found || length-found-rule.callOffset < 5 ||
             bytes[found+rule.callOffset] != 0xE8)) return false;
        // Find spans must not overlap, even if individually unique.
        for (size_t prior=0;prior<r;++prior)
            if (found < offsets[prior]+set.rules[prior].length &&
                offsets[prior] < found+rule.length) return false;
        offsets[r]=found;
    }
    return true;
}
static inline int64_t relativeDelta(const uint8_t *call) {
    const uint32_t raw=uint32_t(call[1]) | uint32_t(call[2])<<8 |
        uint32_t(call[3])<<16 | uint32_t(call[4])<<24;
    return raw & 0x80000000U ? int64_t(raw)-0x100000000LL : int64_t(raw);
}
static inline bool targetAndRange(const uint8_t *call, uint64_t callAddress,
    uint64_t expectedOriginal, uint64_t replacement) {
    if (call == nullptr || call[0] != 0xE8 || callAddress > (~uint64_t(0))-5) return false;
    const uint64_t next=callAddress+5;
    const int64_t originalDelta=relativeDelta(call);
    if (originalDelta < 0 ? next < uint64_t(-originalDelta) :
        next > (~uint64_t(0))-uint64_t(originalDelta)) return false;
    const uint64_t original=originalDelta < 0 ? next-uint64_t(-originalDelta) :
        next+uint64_t(originalDelta);
    if (original != expectedOriginal) return false;
    return replacement >= next ? replacement-next <= 0x7FFFFFFFULL :
        next-replacement <= 0x80000000ULL;
}

// A rebuilt Intel KC may route an external call through one FF 25 disp32
// branch stub. Accept ONLY a no-fault read of that exact instruction and an
// aligned GOT slot containing the canonical IOSleep pointer. No branch chains,
// guessed targets, unbounded reads, or relaxed replacement-distance checks.
using ReadBytes = bool (*)(uint64_t, void *, size_t);
static inline bool addSigned32(uint64_t base, int64_t delta, uint64_t &result) {
    if (delta < -2147483648LL || delta > 2147483647LL) return false;
    if (delta < 0) {
        if (base < uint64_t(-delta)) return false;
        result=base-uint64_t(-delta);
    } else {
        if (base > (~uint64_t(0))-uint64_t(delta)) return false;
        result=base+uint64_t(delta);
    }
    return true;
}
static inline bool sleepTargetAndRange(const uint8_t *call, uint64_t callAddress,
    uint64_t expectedSleep, uint64_t replacement, ReadBytes read) {
    if (targetAndRange(call,callAddress,expectedSleep,replacement)) return true;
    if (!read || !call || call[0]!=0xE8 || callAddress>(~uint64_t(0))-5) return false;
    const auto next=callAddress+5;
    uint64_t stub=0;
    if (!addSigned32(next,relativeDelta(call),stub) ||
        !targetAndRange(call,callAddress,stub,replacement)) return false;
    constexpr uint64_t kernelMinimum=0xFFFF800000000000ULL;
    if (expectedSleep<kernelMinimum || stub<kernelMinimum ||
        stub>(~uint64_t(0))-6) return false;
    uint8_t jump[6] {};
    if (!read(stub,jump,sizeof(jump)) || jump[0]!=0xFF || jump[1]!=0x25) return false;
    // relativeDelta decodes bytes 1..4, so prepend a harmless synthetic opcode.
    const uint8_t displacement[] {0,jump[2],jump[3],jump[4],jump[5]};
    uint64_t slot=0;
    if (!addSigned32(stub+6,relativeDelta(displacement),slot) ||
        slot<kernelMinimum || slot>(~uint64_t(0))-8 || (slot&7)!=0) return false;
    uint64_t resolved=0;
    return read(slot,&resolved,sizeof(resolved)) && resolved==expectedSleep;
}
} // namespace RazerOLEDPatchPlan

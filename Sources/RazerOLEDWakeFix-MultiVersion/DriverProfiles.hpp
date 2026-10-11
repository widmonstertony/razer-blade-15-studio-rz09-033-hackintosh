// MIT. Offline candidate: never install without USB/isolated boot qualification.
#pragma once

namespace RazerOLEDProfiles {
enum class Qualification : uint8_t { LegacyOwnerValidated, IsolatedValidationPending };
struct Profile {
    const char *name;
    uint8_t uuid[16];
    unsigned darwinMinor;
    Qualification qualification;
};
static constexpr Profile profiles[] {
    {"26.2-25C56-v61", {0x0A,0x97,0x00,0x05,0x96,0x6B,0x39,0xC9,
        0x9D,0x30,0x1E,0x8F,0x9B,0xF6,0xF8,0x6A}, 2,
        Qualification::LegacyOwnerValidated},
    {"26.7.1-25G241-offline-candidate", {0x7E,0x29,0x4F,0x0F,0x85,0x2E,0x32,0x52,
        0xB5,0x4D,0x61,0xD9,0xA7,0xBB,0xE6,0xB2}, 6,
        Qualification::IsolatedValidationPending},
};
static inline uint32_t read32(const uint8_t *p) {
    return uint32_t(p[0]) | uint32_t(p[1]) << 8 |
        uint32_t(p[2]) << 16 | uint32_t(p[3]) << 24;
}
// Bounds-checked LC_UUID reader shared verbatim with host tests. No kernel writes.
static inline const Profile *select(const uint8_t *image, size_t length) {
    if (image == nullptr || length < 32 || read32(image) != 0xFEEDFACF ||
        read32(image + 4) != 0x01000007 || read32(image + 12) != 11)
        return nullptr;
    const uint32_t count = read32(image + 16), bytes = read32(image + 20);
    if (count > 512 || bytes > 65536 || bytes > length - 32)
        return nullptr;
    size_t pos = 32, end = 32 + bytes;
    const uint8_t *uuid = nullptr;
    for (uint32_t i = 0; i < count; ++i) {
        if (end - pos < 8) return nullptr;
        const uint32_t cmd = read32(image + pos), n = read32(image + pos + 4);
        if (n < 8 || (n & 7) != 0 || n > end - pos) return nullptr;
        if (cmd == 0x1B) {
            if (n != 24 || uuid != nullptr) return nullptr;
            uuid = image + pos + 8;
        }
        pos += n;
    }
    if (pos != end || uuid == nullptr) return nullptr;
    for (const auto &profile : profiles) {
        bool same = true;
        for (size_t i = 0; i < 16; ++i) same = same && uuid[i] == profile.uuid[i];
        if (same) return &profile;
    }
    return nullptr;
}
// Opt-in is a research switch, never an assertion of successful live qualification.
static inline bool eligible(const Profile *profile, unsigned major, unsigned minor,
    bool isolatedTestOptIn=false) {
    return profile != nullptr && major == 25 && minor == profile->darwinMinor &&
        (profile->qualification == Qualification::LegacyOwnerValidated ||
         (isolatedTestOptIn && profile == &profiles[1]));
}
} // namespace RazerOLEDProfiles

#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <fstream>
#include <vector>
#include <iostream>
#include "DriverProfiles.hpp"
#include "PatchPlan.hpp"
using namespace RazerOLEDPatchPlan;

static constexpr uint64_t fakeStub=0xFFFFFF7F8001002AULL;
static constexpr uint64_t fakeSlot=0xFFFFFF7F80024038ULL;
static constexpr uint64_t fakeSleep=0xFFFFFF800199EAF0ULL;
static uint8_t fakeJump[] {0xFF,0x25,0x08,0x40,0x01,0x00};
static uint64_t fakeResolved=fakeSleep;
static bool allowStub=true,allowSlot=true;
static bool fixtureRead(uint64_t address,void *out,size_t length) {
    if (address==fakeStub && length==sizeof(fakeJump) && allowStub) {
        std::memcpy(out,fakeJump,length);return true;
    }
    if (address==fakeSlot && length==sizeof(fakeResolved) && allowSlot) {
        std::memcpy(out,&fakeResolved,length);return true;
    }
    return false;
}
static void branchStubTests() {
    // The actual 25G241 rebuilt KC uses this FF25 displacement and GOT offset.
    constexpr uint64_t site=0xFFFFFF7F93FA85B0ULL;
    const int32_t delta=static_cast<int32_t>(fakeStub-(site+5));
    uint8_t call[] {0xE8,0,0,0,0};std::memcpy(call+1,&delta,sizeof(delta));
    const auto replacement=site+0x10000000;
    assert(!targetAndRange(call,site,fakeSleep,replacement));
    assert(sleepTargetAndRange(call,site,fakeSleep,replacement,fixtureRead));
    assert(!sleepTargetAndRange(call,site,fakeSleep,replacement,nullptr));
    assert(!sleepTargetAndRange(call,site,fakeSleep,site+5+0x80000000ULL,fixtureRead));
    fakeJump[0]=0xE9;assert(!sleepTargetAndRange(call,site,fakeSleep,replacement,fixtureRead));fakeJump[0]=0xFF;
    fakeJump[1]=0x15;assert(!sleepTargetAndRange(call,site,fakeSleep,replacement,fixtureRead));fakeJump[1]=0x25;
    fakeJump[2]^=1;assert(!sleepTargetAndRange(call,site,fakeSleep,replacement,fixtureRead));fakeJump[2]^=1;
    fakeResolved=fakeSleep+1;assert(!sleepTargetAndRange(call,site,fakeSleep,replacement,fixtureRead));fakeResolved=fakeSleep;
    allowStub=false;assert(!sleepTargetAndRange(call,site,fakeSleep,replacement,fixtureRead));allowStub=true;
    allowSlot=false;assert(!sleepTargetAndRange(call,site,fakeSleep,replacement,fixtureRead));allowSlot=true;
    assert(!sleepTargetAndRange(call,site,0x1000,replacement,fixtureRead));
    assert(!sleepTargetAndRange(nullptr,site,fakeSleep,replacement,fixtureRead));
    assert(!sleepTargetAndRange(call,~uint64_t(0),fakeSleep,replacement,fixtureRead));
    uint64_t result=0;
    assert(!addSigned32(0,-1,result));
    assert(!addSigned32(~uint64_t(0),1,result));
    assert(!addSigned32(0,2147483648LL,result));
    assert(!addSigned32(0,-2147483649LL,result));
    assert(addSigned32(5,-5,result) && result==0);
    const uint8_t direct[] {0xE8,0x10,0,0,0};
    assert(sleepTargetAndRange(direct,0x1000,0x1015,0x2000,nullptr));
}

static std::vector<uint8_t> fixture(const Set &set) {
    std::vector<uint8_t> bytes;
    for (size_t i=0;i<ruleCount;++i) {
        const auto &r=set.rules[i];
        bytes.insert(bytes.end(),64,0xCC);
        const size_t start=bytes.size();
        size_t extent=r.length;
        for (size_t j=0;j<r.suffixCount;++j)
            if (r.suffix[j].offset+1 > extent) extent=r.suffix[j].offset+1;
        if (r.target != Target::None && r.callOffset+5 > extent) extent=r.callOffset+5;
        bytes.resize(start+extent,0xCC);
        std::memcpy(bytes.data()+start,r.find,r.length);
        for (size_t j=0;j<r.suffixCount;++j) bytes[start+r.suffix[j].offset]=r.suffix[j].value;
        if (r.target != Target::None) bytes[start+r.callOffset]=0xE8;
    }
    return bytes;
}
static void negatives(const Set &set) {
    auto good=fixture(set);
    size_t offsets[ruleCount] {};
    assert(preflight(set,good.data(),good.size(),offsets));
    assert(!preflight(set,nullptr,good.size(),offsets));
    for (size_t r=0;r<ruleCount;++r) {
        // Every byte in every exact prefix must be checked.
        for (size_t j=0;j<set.rules[r].length;++j) {
            auto bad=good; bad[offsets[r]+j]^=1;
            size_t out[ruleCount] {};
            assert(!preflight(set,bad.data(),bad.size(),out));
        }
        auto duplicate=good;
        duplicate.insert(duplicate.end(),set.rules[r].find,set.rules[r].find+set.rules[r].length);
        size_t out[ruleCount] {};
        assert(!preflight(set,duplicate.data(),duplicate.size(),out));
        for (size_t j=0;j<set.rules[r].suffixCount;++j) {
            auto bad=good; bad[offsets[r]+set.rules[r].suffix[j].offset]^=1;
            assert(!preflight(set,bad.data(),bad.size(),out));
        }
    }
    for (size_t n=0;n<good.size();++n) {
        size_t out[ruleCount] {};
        // Some trailing suffix bytes are not required; truncate before last required byte.
        const auto &last=set.rules[ruleCount-1];
        size_t required=last.length;
        for (size_t j=0;j<last.suffixCount;++j)
            if (last.suffix[j].offset+1>required) required=last.suffix[j].offset+1;
        if (n<offsets[ruleCount-1]+required) assert(!preflight(set,good.data(),n,out));
    }
}
int main(int argc,char **argv) {
    branchStubTests();
    negatives(legacySet); negatives(newSet);
    const uint8_t forward[] {0xE8,0x10,0,0,0};
    assert(targetAndRange(forward,0x1000,0x1015,0x1005+0x7FFFFFFFULL));
    assert(!targetAndRange(forward,0x1000,0x1015,0x1005+0x80000000ULL));
    assert(!targetAndRange(forward,0x1000,0x1014,0x2000));
    assert(targetAndRange(forward,0x80000000,0x80000015,5));
    assert(!targetAndRange(forward,0x80000000,0x80000015,4));
    assert(!targetAndRange(forward,~uint64_t(0),0,0));
    const uint8_t backward[] {0xE8,0xF0,0xFF,0xFF,0xFF};
    assert(targetAndRange(backward,0x1000,0xFF5,0x2000));
    assert(!targetAndRange(backward,0,0,0));
    for (int i=1;i<argc;++i) {
        std::ifstream stream(argv[i],std::ios::binary); assert(stream.good());
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)),{});
        const auto *profile=RazerOLEDProfiles::select(bytes.data(),bytes.size()); assert(profile);
        const auto &set=profile->darwinMinor==6?newSet:legacySet;
        size_t offsets[ruleCount] {};
        assert(preflight(set,bytes.data(),bytes.size(),offsets));
        const auto &other=profile->darwinMinor==6?legacySet:newSet;
        assert(!preflight(other,bytes.data(),bytes.size(),offsets));
        std::cout << profile->name << ": 10 unique prefixes and suffixes PASS\n";
    }
    std::cout << "PASS: exact patterns, duplicates, mutations, truncation, call target, rel32 limits and strict single IOSleep KC stub\n";
}

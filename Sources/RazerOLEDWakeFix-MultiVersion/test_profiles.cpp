#include <cstdint>
#include <cstddef>
#include <cassert>
#include <cstring>
#include <fstream>
#include <vector>
#include <iostream>
#include "DriverProfiles.hpp"
using namespace RazerOLEDProfiles;
static std::vector<uint8_t> image(unsigned profile) {
    std::vector<uint8_t> d(56);
    auto put = [&](size_t p, uint32_t v) { for (unsigned i=0;i<4;++i) d[p+i]=v>>(8*i); };
    put(0,0xFEEDFACF); put(4,0x01000007); put(12,11);
    put(16,1); put(20,24); put(32,0x1B); put(36,24);
    std::memcpy(d.data()+40,profiles[profile].uuid,16);
    return d;
}
int main(int argc, char **argv) {
    auto a=image(0), b=image(1);
    assert(eligible(select(a.data(),a.size()),25,2));
    assert(!eligible(select(a.data(),a.size()),25,6));
    assert(select(b.data(),b.size())==&profiles[1]);
    assert(!eligible(select(b.data(),b.size()),25,6));
    assert(eligible(select(b.data(),b.size()),25,6,true));
    assert(!eligible(select(b.data(),b.size()),25,2,true));
    assert(!eligible(select(b.data(),b.size()),26,6,true));
    assert(!eligible(nullptr,25,6,true));
    assert(select(nullptr,100)==nullptr);
    for (size_t n=0;n<a.size();++n) assert(select(a.data(),n)==nullptr);
    for (size_t i=40;i<56;++i) { auto bad=a; bad[i]^=1; assert(select(bad.data(),bad.size())==nullptr); }
    for (size_t i : {size_t(0),size_t(4),size_t(12),size_t(16),size_t(20),size_t(36)}) {
        auto bad=a; bad[i]=0xFF; assert(select(bad.data(),bad.size())==nullptr);
    }
    auto duplicate=a; duplicate.insert(duplicate.end(),a.begin()+32,a.end());
    duplicate[16]=2; duplicate[20]=48;
    assert(select(duplicate.data(),duplicate.size())==nullptr);
    auto missing=a; missing[32]=0x19; assert(select(missing.data(),missing.size())==nullptr);
    for (int i=1;i<argc;++i) {
        std::ifstream stream(argv[i],std::ios::binary);
        assert(stream.good());
        std::vector<uint8_t> d((std::istreambuf_iterator<char>(stream)),{});
        const auto *p=select(d.data(),d.size()); assert(p);
        std::cout << argv[i] << ": " << p->name << "; eligible=" << eligible(p,25,p->darwinMinor) << "\n";
    }
    std::cout << "PASS: identity, version, pending-port, malformed-header and unknown-UUID tests\n";
}

#include <cassert>
#include <iostream>
#include "SleepPhaseGate.hpp"
int main() {
    using RazerOLEDSleepPhaseGate::allowPostPowerWellRestore;
    unsigned tests=0;
    for (unsigned minor : {2U,6U,7U})
    for (bool panel : {false,true})
    for (bool pending : {false,true})
    for (bool boundary : {false,true})
    for (bool terminating : {false,true}) {
        const bool expected=panel&&(minor==2||(minor==6&&!terminating&&(!pending||boundary)));
        assert(allowPostPowerWellRestore(minor,panel,pending,boundary,terminating)==expected);
        ++tests;
    }
    // Captured 2026-10-12: will-sleep, panel-off armed, no FB0 resume boundary.
    assert(!allowPostPowerWellRestore(6,true,true,false,false));
    assert(allowPostPowerWellRestore(6,true,false,false,false));
    assert(allowPostPowerWellRestore(6,true,true,true,false));
    assert(!allowPostPowerWellRestore(6,true,false,true,true));
    std::cout << "PASS: " << tests << " phase cases; captured suspend write deferred; legacy behavior preserved\n";
}

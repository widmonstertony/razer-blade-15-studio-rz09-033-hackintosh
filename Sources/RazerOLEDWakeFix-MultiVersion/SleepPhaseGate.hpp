#pragma once
// Host-testable gate. Does not prove the cause of the observed black screen.
namespace RazerOLEDSleepPhaseGate {
static inline bool allowPostPowerWellRestore(unsigned minor, bool panelNeedsRestore,
    bool sleepPending, bool resumeBoundaryObserved, bool terminating) {
    if (!panelNeedsRestore) return false;
    if (minor == 2) return true; // Preserve the owner's legacy V61 behavior.
    if (minor != 6) return false;
    // An enable callback can run on the suspend side as well as the resume side.
    // Do not re-latch the awake clock/lane capability during committed suspend.
    return !terminating && (!sleepPending || resumeBoundaryObserved);
}
}

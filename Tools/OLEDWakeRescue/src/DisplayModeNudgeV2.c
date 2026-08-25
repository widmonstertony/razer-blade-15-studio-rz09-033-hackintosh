#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const uint32_t kExpectedVendor = 19587;
static const uint32_t kExpectedProduct = 41001;

static int close_to_60(CGDisplayModeRef mode) {
    return fabs(CGDisplayModeGetRefreshRate(mode) - 60.0) < 0.1;
}

static void describe_mode(const char *label, CGDisplayModeRef mode) {
    printf("%s modeID=0x%x logical=%zux%zu backing=%zux%zu refresh=%.3f "
           "flags=0x%x usable=%d\n",
           label,
           (unsigned int)CGDisplayModeGetIODisplayModeID(mode),
           CGDisplayModeGetWidth(mode), CGDisplayModeGetHeight(mode),
           CGDisplayModeGetPixelWidth(mode), CGDisplayModeGetPixelHeight(mode),
           CGDisplayModeGetRefreshRate(mode),
           (unsigned int)CGDisplayModeGetIOFlags(mode),
           CGDisplayModeIsUsableForDesktopGUI(mode));
}

static CGError commit_mode(CGDirectDisplayID display, CGDisplayModeRef mode) {
    CGDisplayConfigRef config = NULL;
    CGError result = CGBeginDisplayConfiguration(&config);
    if (result != kCGErrorSuccess || config == NULL) {
        return result;
    }

    result = CGConfigureDisplayWithDisplayMode(config, display, mode, NULL);
    if (result != kCGErrorSuccess) {
        CGCancelDisplayConfiguration(config);
        return result;
    }

    return CGCompleteDisplayConfiguration(config, kCGConfigureForAppOnly);
}

static void fail(const char *message, int code) {
    fprintf(stderr, "ERROR: %s\n", message);
    exit(code);
}

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "--probe") != 0 &&
                      strcmp(argv[1], "--nudge") != 0)) {
        fail("usage: DisplayModeNudgeV2 --probe|--nudge", 64);
    }

    uint32_t count = 0;
    if (CGGetOnlineDisplayList(0, NULL, &count) != kCGErrorSuccess || count == 0) {
        fail("cannot enumerate online displays", 1);
    }
    CGDirectDisplayID *displays = calloc(count, sizeof(CGDirectDisplayID));
    if (displays == NULL) fail("cannot allocate display list", 1);
    if (CGGetOnlineDisplayList(count, displays, &count) != kCGErrorSuccess) {
        free(displays);
        fail("cannot read online display IDs", 1);
    }

    CGDirectDisplayID display = 0;
    unsigned int matches = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (CGDisplayIsBuiltin(displays[i]) &&
            CGDisplayVendorNumber(displays[i]) == kExpectedVendor &&
            CGDisplayModelNumber(displays[i]) == kExpectedProduct) {
            display = displays[i];
            matches++;
        }
    }
    free(displays);
    if (matches != 1) fail("expected exactly one Samsung 4C83:A029 built-in display", 1);

    CGDisplayModeRef original = CGDisplayCopyDisplayMode(display);
    if (original == NULL) fail("cannot copy current display mode", 1);
    if (CGDisplayModeGetWidth(original) != 1680 ||
        CGDisplayModeGetHeight(original) != 945 ||
        CGDisplayModeGetPixelWidth(original) != 3360 ||
        CGDisplayModeGetPixelHeight(original) != 1890 ||
        !close_to_60(original)) {
        describe_mode("unexpected-current", original);
        CFRelease(original);
        fail("current display mode is outside the protected baseline", 1);
    }

    const void *keys[] = { kCGDisplayShowDuplicateLowResolutionModes };
    const void *values[] = { kCFBooleanTrue };
    CFDictionaryRef options = CFDictionaryCreate(kCFAllocatorDefault, keys, values, 1,
                                                 &kCFTypeDictionaryKeyCallBacks,
                                                 &kCFTypeDictionaryValueCallBacks);
    CFArrayRef modes = CGDisplayCopyAllDisplayModes(display, options);
    CFRelease(options);
    if (modes == NULL) {
        CFRelease(original);
        fail("cannot enumerate display modes", 1);
    }

    CGDisplayModeRef rescue = NULL;
    CFIndex mode_count = CFArrayGetCount(modes);
    for (CFIndex i = 0; i < mode_count; i++) {
        CGDisplayModeRef candidate = (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, i);
        if (CGDisplayModeGetWidth(candidate) == 1600 &&
            CGDisplayModeGetHeight(candidate) == 900 &&
            CGDisplayModeGetPixelWidth(candidate) == 3200 &&
            CGDisplayModeGetPixelHeight(candidate) == 1800 &&
            close_to_60(candidate) &&
            CGDisplayModeIsUsableForDesktopGUI(candidate)) {
            if (rescue != NULL &&
                (CGDisplayModeGetIODisplayModeID(rescue) != CGDisplayModeGetIODisplayModeID(candidate) ||
                 CGDisplayModeGetIOFlags(rescue) != CGDisplayModeGetIOFlags(candidate))) {
                CFRelease(modes);
                CFRelease(original);
                fail("ambiguous non-identical rescue modes were returned", 1);
            }
            rescue = candidate;
        }
    }
    if (rescue == NULL) {
        CFRelease(modes);
        CFRelease(original);
        fail("protected usable 1600x900/3200x1800 rescue mode is unavailable", 1);
    }
    CFRetain(rescue);
    CFRelease(modes);

    printf("display=%u vendor=%u product=%u\n", display,
           CGDisplayVendorNumber(display), CGDisplayModelNumber(display));
    describe_mode("original", original);
    describe_mode("rescue  ", rescue);
    fflush(stdout);

    if (strcmp(argv[1], "--probe") == 0) {
        printf("PROBE_V2_PASS: no display state changed\n");
        CFRelease(rescue);
        CFRelease(original);
        return 0;
    }

    CGError result = commit_mode(display, rescue);
    if (result != kCGErrorSuccess) {
        fprintf(stderr, "ERROR: rescue-mode transaction failed with CGError %d\n", result);
        CFRelease(rescue);
        CFRelease(original);
        return 1;
    }
    usleep(750000);

    CGError restore_result = commit_mode(display, original);
    for (int attempt = 0; attempt < 2 && restore_result != kCGErrorSuccess; attempt++) {
        usleep(500000);
        restore_result = commit_mode(display, original);
    }
    if (restore_result != kCGErrorSuccess) {
        fprintf(stderr, "ERROR: original-mode transaction failed with CGError %d\n", restore_result);
        CFRelease(rescue);
        CFRelease(original);
        return 2;
    }
    usleep(750000);
    CGDisplayRestoreColorSyncSettings();

    CGDisplayModeRef final = CGDisplayCopyDisplayMode(display);
    int restored = final != NULL &&
        CGDisplayModeGetWidth(final) == 1680 &&
        CGDisplayModeGetHeight(final) == 945 &&
        CGDisplayModeGetPixelWidth(final) == 3360 &&
        CGDisplayModeGetPixelHeight(final) == 1890 &&
        close_to_60(final);
    if (final != NULL) describe_mode("final   ", final);
    if (final != NULL) CFRelease(final);
    CFRelease(rescue);
    CFRelease(original);
    if (!restored) fail("final display mode did not return to the protected baseline", 3);

    printf("NUDGE_V2_PASS: display transaction completed and exact original mode restored\n");
    return 0;
}

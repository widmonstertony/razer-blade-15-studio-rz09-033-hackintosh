#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreGraphics/CGSession.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const uint32_t kExpectedVendor = 19587;
static const uint32_t kExpectedProduct = 41001;

static void fail(const char *message, int code) {
    fprintf(stderr, "ERROR: %s\n", message);
    exit(code);
}

static int close_to_60(CGDisplayModeRef mode) {
    return fabs(CGDisplayModeGetRefreshRate(mode) - 60.0) < 0.1;
}

static void describe_mode(const char *label, CGDisplayModeRef mode) {
    CFStringRef encoding = CGDisplayModeCopyPixelEncoding(mode);
    char encoding_buffer[128] = "(unknown)";
    if (encoding != NULL) {
        CFStringGetCString(encoding, encoding_buffer, sizeof(encoding_buffer),
                           kCFStringEncodingUTF8);
    }
    printf("%s modeID=0x%x logical=%zux%zu backing=%zux%zu refresh=%.3f "
           "flags=0x%x usable=%d encoding=%s\n",
           label,
           (unsigned int)CGDisplayModeGetIODisplayModeID(mode),
           CGDisplayModeGetWidth(mode), CGDisplayModeGetHeight(mode),
           CGDisplayModeGetPixelWidth(mode), CGDisplayModeGetPixelHeight(mode),
           CGDisplayModeGetRefreshRate(mode),
           (unsigned int)CGDisplayModeGetIOFlags(mode),
           CGDisplayModeIsUsableForDesktopGUI(mode), encoding_buffer);
    if (encoding != NULL) CFRelease(encoding);
}

static void describe_session(const char *label) {
    CFDictionaryRef session = CGSessionCopyCurrentDictionary();
    if (session == NULL) {
        printf("%s session=(unavailable)\n", label);
        return;
    }
    CFTypeRef locked = CFDictionaryGetValue(session, CFSTR("CGSSessionScreenIsLocked"));
    CFTypeRef on_console = CFDictionaryGetValue(session, kCGSessionOnConsoleKey);
    CFTypeRef login_done = CFDictionaryGetValue(session, kCGSessionLoginDoneKey);
    printf("%s locked=%s onConsole=%s loginDone=%s\n", label,
           locked == kCFBooleanTrue ? "true" :
               locked == kCFBooleanFalse ? "false" : "unknown",
           on_console == kCFBooleanTrue ? "true" :
               on_console == kCFBooleanFalse ? "false" : "unknown",
           login_done == kCFBooleanTrue ? "true" :
               login_done == kCFBooleanFalse ? "false" : "unknown");
    CFRelease(session);
}

static int same_encoding(CGDisplayModeRef left, CGDisplayModeRef right) {
    CFStringRef left_encoding = CGDisplayModeCopyPixelEncoding(left);
    CFStringRef right_encoding = CGDisplayModeCopyPixelEncoding(right);
    int equal = left_encoding != NULL && right_encoding != NULL &&
                CFEqual(left_encoding, right_encoding);
    if (left_encoding != NULL) CFRelease(left_encoding);
    if (right_encoding != NULL) CFRelease(right_encoding);
    return equal;
}

static int same_baseline(CGDisplayModeRef candidate, CGDisplayModeRef baseline) {
    return CGDisplayModeGetWidth(candidate) == CGDisplayModeGetWidth(baseline) &&
           CGDisplayModeGetHeight(candidate) == CGDisplayModeGetHeight(baseline) &&
           CGDisplayModeGetPixelWidth(candidate) == CGDisplayModeGetPixelWidth(baseline) &&
           CGDisplayModeGetPixelHeight(candidate) == CGDisplayModeGetPixelHeight(baseline) &&
           fabs(CGDisplayModeGetRefreshRate(candidate) -
                CGDisplayModeGetRefreshRate(baseline)) < 0.1 &&
           CGDisplayModeGetIOFlags(candidate) == CGDisplayModeGetIOFlags(baseline) &&
           same_encoding(candidate, baseline);
}

static CGError commit_mode(CGDirectDisplayID display, CGDisplayModeRef mode) {
    CGDisplayConfigRef config = NULL;
    CGError result = CGBeginDisplayConfiguration(&config);
    if (result != kCGErrorSuccess || config == NULL) return result;

    result = CGConfigureDisplayWithDisplayMode(config, display, mode, NULL);
    if (result != kCGErrorSuccess) {
        CGCancelDisplayConfiguration(config);
        return result;
    }

    /* Session scope avoids the implicit third reconfiguration that app-only
       scope performs when this short-lived helper exits. */
    return CGCompleteDisplayConfiguration(config, kCGConfigureForSession);
}

int main(int argc, char **argv) {
    if (argc != 2 || (strcmp(argv[1], "--probe") != 0 &&
                      strcmp(argv[1], "--nudge") != 0)) {
        fail("usage: DisplayModeNudgeV3 --probe|--nudge", 64);
    }

    uint32_t count = 0;
    if (CGGetOnlineDisplayList(0, NULL, &count) != kCGErrorSuccess || count == 0) {
        fail("cannot enumerate online displays", 1);
    }
    CGDirectDisplayID *displays = calloc(count, sizeof(*displays));
    if (displays == NULL ||
        CGGetOnlineDisplayList(count, displays, &count) != kCGErrorSuccess) {
        free(displays);
        fail("cannot read online display IDs", 1);
    }

    CGDirectDisplayID display = 0;
    unsigned int matches = 0;
    for (uint32_t index = 0; index < count; index++) {
        if (CGDisplayIsBuiltin(displays[index]) &&
            CGDisplayVendorNumber(displays[index]) == kExpectedVendor &&
            CGDisplayModelNumber(displays[index]) == kExpectedProduct) {
            display = displays[index];
            matches++;
        }
    }
    free(displays);
    if (matches != 1) {
        fail("expected exactly one Samsung 4C83:A029 built-in display", 1);
    }

    CGDisplayModeRef original = CGDisplayCopyDisplayMode(display);
    if (original == NULL) fail("cannot copy current display mode", 1);
    if (CGDisplayModeGetWidth(original) != 1680 ||
        CGDisplayModeGetHeight(original) != 945 ||
        CGDisplayModeGetPixelWidth(original) != 3360 ||
        CGDisplayModeGetPixelHeight(original) != 1890 || !close_to_60(original)) {
        describe_mode("unexpected-current", original);
        CFRelease(original);
        fail("current display mode is outside the protected baseline", 1);
    }

    const void *keys[] = {kCGDisplayShowDuplicateLowResolutionModes};
    const void *values[] = {kCFBooleanTrue};
    CFDictionaryRef options = CFDictionaryCreate(
        kCFAllocatorDefault, keys, values, 1,
        &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFArrayRef modes = CGDisplayCopyAllDisplayModes(display, options);
    CFRelease(options);
    if (modes == NULL) {
        CFRelease(original);
        fail("cannot enumerate display modes", 1);
    }

    CGDisplayModeRef rescue = NULL;
    CGDisplayModeRef restore = NULL;
    unsigned int rescue_matches = 0;
    unsigned int restore_matches = 0;
    for (CFIndex index = 0; index < CFArrayGetCount(modes); index++) {
        CGDisplayModeRef candidate =
            (CGDisplayModeRef)CFArrayGetValueAtIndex(modes, index);
        if (!CGDisplayModeIsUsableForDesktopGUI(candidate)) continue;

        if (CGDisplayModeGetWidth(candidate) == 1600 &&
            CGDisplayModeGetHeight(candidate) == 900 &&
            CGDisplayModeGetPixelWidth(candidate) == 3200 &&
            CGDisplayModeGetPixelHeight(candidate) == 1800 && close_to_60(candidate) &&
            CGDisplayModeGetIOFlags(candidate) == CGDisplayModeGetIOFlags(original) &&
            same_encoding(candidate, original)) {
            rescue = candidate;
            rescue_matches++;
        }
        if (same_baseline(candidate, original)) {
            restore = candidate;
            restore_matches++;
        }
    }
    if (rescue_matches != 1 || restore_matches != 1) {
        CFRelease(modes);
        CFRelease(original);
        fail("expected one usable rescue mode and one usable equivalent restore mode", 1);
    }
    CFRetain(rescue);
    CFRetain(restore);
    CFRelease(modes);

    printf("display=%u vendor=%u product=%u\n", display,
           CGDisplayVendorNumber(display), CGDisplayModelNumber(display));
    describe_session("session-before");
    describe_mode("original", original);
    describe_mode("rescue  ", rescue);
    describe_mode("restore ", restore);
    fflush(stdout);

    if (strcmp(argv[1], "--probe") == 0) {
        printf("PROBE_V3_PASS: usable equivalent restore selected; no display state changed\n");
        CFRelease(restore);
        CFRelease(rescue);
        CFRelease(original);
        return 0;
    }

    CGError result = commit_mode(display, rescue);
    if (result != kCGErrorSuccess) {
        fprintf(stderr, "ERROR: rescue-mode session transaction failed with CGError %d\n", result);
        CFRelease(restore);
        CFRelease(rescue);
        CFRelease(original);
        return 1;
    }
    usleep(750000);

    CGError restore_result = commit_mode(display, restore);
    for (int attempt = 0; attempt < 2 && restore_result != kCGErrorSuccess; attempt++) {
        usleep(500000);
        restore_result = commit_mode(display, restore);
    }
    if (restore_result != kCGErrorSuccess) {
        fprintf(stderr, "ERROR: usable restore transaction failed with CGError %d\n",
                restore_result);
        CFRelease(restore);
        CFRelease(rescue);
        CFRelease(original);
        return 2;
    }
    usleep(750000);

    CGDisplayModeRef final = CGDisplayCopyDisplayMode(display);
    int restored = final != NULL && same_baseline(final, restore) &&
                   CGDisplayModeIsUsableForDesktopGUI(final);
    if (final != NULL) describe_mode("final   ", final);
    describe_session("session-after");
    if (final != NULL) CFRelease(final);
    CFRelease(restore);
    CFRelease(rescue);
    CFRelease(original);
    if (!restored) fail("final mode did not resolve to the usable protected baseline", 3);

    printf("NUDGE_V3_PASS: two session transactions completed; usable baseline restored\n");
    return 0;
}

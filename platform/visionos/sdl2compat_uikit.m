// UIKit replacement for sdl2-compat's macOS-only src/sdl2_compat_objc.m.
// sdl2-compat calls these to log and to report a fatal startup error (such as
// SDL3 failing to load). There is no AppKit alert on UIKit, so both go to the
// system log.

#import <Foundation/Foundation.h>

__attribute__((visibility("hidden")))
void SDL2Compat_NSLog(const char *prefix, const char *text)
{
    @autoreleasepool {
        NSString *nsText = [NSString stringWithUTF8String:text ? text : ""];
        if (prefix && *prefix)
            NSLog(@"%s%@", prefix, nsText);
        else
            NSLog(@"%@", nsText);
    }
}

__attribute__((visibility("hidden")))
void error_dialog(const char *errorMsg)
{
    SDL2Compat_NSLog("sdl2-compat error: ", errorMsg);
}

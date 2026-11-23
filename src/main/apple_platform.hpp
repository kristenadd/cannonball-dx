#pragma once
// Apple platform selection shared by the iOS, iPadOS and visionOS ports.
//
//   CB_APPLE_MOBILE  iOS, iPadOS or visionOS (UIKit apps: no quitting, Files-app
//                    ROM folder, menu on start, Select/tap opens the menu)
//   CB_IOS           iPhone and iPad only (landscape full screen, FULL aspect)
//   CB_VISIONOS      Apple Vision Pro (resizable window, no OpenGL ES)

#ifdef __APPLE__
    #include <TargetConditionals.h>
#endif
#ifndef TARGET_OS_VISION
    #define TARGET_OS_VISION 0
#endif

#if defined(__APPLE__) && TARGET_OS_IPHONE
    #define CB_APPLE_MOBILE 1
#else
    #define CB_APPLE_MOBILE 0
#endif

#if defined(__APPLE__) && TARGET_OS_VISION
    #define CB_VISIONOS 1
#else
    #define CB_VISIONOS 0
#endif

#define CB_IOS (CB_APPLE_MOBILE && !CB_VISIONOS)

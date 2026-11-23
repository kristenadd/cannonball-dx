// UIScene lifecycle support for SDL2 on iOS.
//
// iOS 27 terminates apps that do not adopt the UIScene lifecycle (UIKit traps
// in UIApplicationEvaluateRuntimeIssueForNoSceneLifecycleAdoption). SDL2 still
// uses the pre-scene model: it creates its UIWindows (launch screen, game
// window, message boxes) with -initWithFrame:, so they belong to no scene and
// would never be shown.
//
// This file:
//   - provides the scene delegate named in Info.plist (UIApplicationSceneManifest)
//   - gives every UIWindow created without a scene the app's window scene, either
//     immediately or as soon as the scene connects.
//
// Lifecycle events need no forwarding: UIKit still posts the UIApplication
// notifications (did enter background, etc.) that SDL observes.

#import <UIKit/UIKit.h>
#import <objc/runtime.h>

static __weak UIWindowScene* cb_scene = nil;
static NSHashTable<UIWindow*>* cb_pending_windows = nil;   // weak references

static UIWindowScene* cb_active_scene()
{
    if (cb_scene)
        return cb_scene;
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:UIWindowScene.class])
            return (UIWindowScene*)scene;
    }
    return nil;
}

static void cb_adopt_window(UIWindow* window)
{
    if (!window || window.windowScene)
        return;
    UIWindowScene* scene = cb_active_scene();
    if (scene)
        window.windowScene = scene;
    else
        [cb_pending_windows addObject:window];
}

@interface CBSceneDelegate : UIResponder <UIWindowSceneDelegate>
@property (strong, nonatomic) UIWindow* window;
@end

@implementation CBSceneDelegate

- (void)scene:(UIScene*)scene
    willConnectToSession:(UISceneSession*)session
                 options:(UISceneConnectionOptions*)connectionOptions
{
    if (![scene isKindOfClass:UIWindowScene.class])
        return;
    cb_scene = (UIWindowScene*)scene;

    // Windows SDL created before the scene existed (e.g. its launch window).
    for (UIWindow* window in cb_pending_windows.allObjects) {
        if (!window.windowScene)
            window.windowScene = cb_scene;
    }
    [cb_pending_windows removeAllObjects];
}

@end

// Swizzle -[UIWindow initWithFrame:] so scene-less windows join the scene.
static IMP cb_original_init_with_frame = nullptr;

static id cb_init_with_frame(UIWindow* self, SEL cmd, CGRect frame)
{
    self = reinterpret_cast<id (*)(id, SEL, CGRect)>(cb_original_init_with_frame)(self, cmd, frame);
    cb_adopt_window(self);
    return self;
}

@interface UIWindow (CBSceneAdoption)
@end

@implementation UIWindow (CBSceneAdoption)

+ (void)load
{
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        cb_pending_windows = [NSHashTable weakObjectsHashTable];
        Method method = class_getInstanceMethod(UIWindow.class, @selector(initWithFrame:));
        cb_original_init_with_frame = method_setImplementation(method, (IMP)cb_init_with_frame);
    });
}

@end

// visionOS window shape.
//
// The game window keeps the aspect ratio of the selected game image (4:3,
// 16:10, 16:9, 21:9 or, for Stretched, 4:3). visionOS windows are sized by the
// system and the user, so ask the window scene for that shape and restrict
// resizing to uniform scaling, which keeps it while the user resizes.

#import <UIKit/UIKit.h>

static UIWindowScene* cb_window_scene()
{
    for (UIScene* scene in UIApplication.sharedApplication.connectedScenes) {
        if ([scene isKindOfClass:UIWindowScene.class])
            return (UIWindowScene*)scene;
    }
    return nil;
}

extern "C" void cb_visionos_set_window_aspect(double aspect)
{
    if (!(aspect > 0.1 && aspect < 10.0))
        return;

    void (^apply)(void) = ^{
        UIWindowScene* scene = cb_window_scene();
        if (!scene)
            return;

        // Keep the current height (what the user last chose) and fit the width.
        CGSize current = scene.coordinateSpace.bounds.size;
        CGFloat height = current.height > 0 ? current.height : 720.0;
        CGSize size = CGSizeMake(round(height * aspect), height);

        UIWindowSceneGeometryPreferencesVision* prefs =
            [[UIWindowSceneGeometryPreferencesVision alloc] initWithSize:size];
        prefs.resizingRestrictions = UIWindowSceneResizingRestrictionsUniform;
        [scene requestGeometryUpdateWithPreferences:prefs
                                       errorHandler:^(NSError* error) {
            NSLog(@"Window geometry request failed: %@", error);
        }];
    };

    if (NSThread.isMainThread)
        apply();
    else
        dispatch_async(dispatch_get_main_queue(), apply);
}

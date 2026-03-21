#import "AppDelegate.h"

#include <memory>

#import "../../Engine/ArchiverEngine.hpp"
#import "AppConfigStore.hpp"
#import "AppRuntimePaths.hpp"
#import "../UIConstants.hpp"
#import "../ViewControllers/RootViewController.hpp"

namespace peanutbutter {

NSString *NSStringFromStdString(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()] ?: @"";
}

}  // namespace peanutbutter

@implementation AppDelegate {
    RootViewController *_rootViewController;
    AppConfigStore *_configStore;
    peanutbutter::AppConfigStateV2 _configState;
    std::unique_ptr<peanutbutter::ArchiverEngine> _engine;
}

- (NSWindow *)buildWindowIfNeeded {
    if (self.window != nil) {
        return self.window;
    }

    self.window = [[NSWindow alloc] initWithContentRect:NSMakeRect(0.0, 0.0, kUIWindowWidth, kUIWindowHeight)
                                              styleMask:(NSWindowStyleMaskTitled |
                                                         NSWindowStyleMaskClosable |
                                                         NSWindowStyleMaskMiniaturizable |
                                                         NSWindowStyleMaskResizable)
                                                backing:NSBackingStoreBuffered
                                                  defer:NO];
    self.window.contentMinSize = NSMakeSize(kUIWindowMinWidth, kUIWindowMinHeight);
    self.window.contentMaxSize = NSMakeSize(kUIWindowMaxWidth, kUIWindowMaxHeight);
    return self.window;
}

- (void)applicationDidFinishLaunching:(NSNotification *)aNotification {
    (void)aNotification;

    if ([AppRuntimePaths shouldUseAppContainerAsWorkingDirectory]) {
        [AppRuntimePaths applyAppContainerAsCurrentWorkingDirectory];
    }

    NSWindow *window = [self buildWindowIfNeeded];
    window.title = @"PeanutButter Archiver";
    window.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];
    NSApp.appearance = [NSAppearance appearanceNamed:NSAppearanceNameAqua];

    _rootViewController = [[RootViewController alloc] init];
    _configStore = [[AppConfigStore alloc] init];
    _configState = [_configStore loadOrCreateConfig];
    _rootViewController.configStore = _configStore;
    _rootViewController.homeTabDefault = static_cast<NSInteger>(_configState.mHomeTab);
    _rootViewController.bundleSourceDefault =
        _configState.mBundleDirectorySource.empty()
            ? @"input"
            : peanutbutter::NSStringFromStdString(_configState.mBundleDirectorySource);
    _rootViewController.bundleDestinationDefault =
        _configState.mBundleDirectoryDestination.empty()
            ? @"archive"
            : peanutbutter::NSStringFromStdString(_configState.mBundleDirectoryDestination);
    _rootViewController.unbundleSourceDefault =
        _configState.mUnbundleDirectorySource.empty()
            ? @"archive"
            : peanutbutter::NSStringFromStdString(_configState.mUnbundleDirectorySource);
    _rootViewController.unbundleDestinationDefault =
        _configState.mUnbundleDirectoryDestination.empty()
            ? @"unbundled"
            : peanutbutter::NSStringFromStdString(_configState.mUnbundleDirectoryDestination);
    _rootViewController.bundleFilePrefixDefault =
        _configState.mBundleFilePrefix.empty()
            ? @"archive"
            : peanutbutter::NSStringFromStdString(_configState.mBundleFilePrefix);
    _rootViewController.bundlePasswordDefault =
        peanutbutter::NSStringFromStdString(_configState.mBundlePassword);
    _rootViewController.unbundlePasswordDefault =
        peanutbutter::NSStringFromStdString(_configState.mUnbundlePassword);
    _rootViewController.bundleBlockCountDefault =
        peanutbutter::NSStringFromStdString(_configState.mBundleBlockCount);
    _rootViewController.bundleEncryptionStrengthDefault =
        peanutbutter::NSStringFromStdString(_configState.mBundleEncryptionStrength);
    _rootViewController.bundleTableStrengthDefault =
        peanutbutter::NSStringFromStdString(_configState.mBundleTableStrength);
    _rootViewController.bundleRepairSizeDefault =
        peanutbutter::NSStringFromStdString(_configState.mBundleRepairSize);
    _rootViewController.bundleRepairDefault = _configState.mBundleRepair;
    _rootViewController.bundleSafeDefault = _configState.mBundleSafe;
    _rootViewController.bundleEncryptDefault = _configState.mBundleEncrypt;
    _rootViewController.bundleIncludePreviewDefault = _configState.mBundleIncludePreview;

    window.contentViewController = _rootViewController;
    [window center];
    [window makeKeyAndOrderFront:nil];

    _engine = std::make_unique<peanutbutter::ArchiverEngine>();
}


- (void)applicationWillTerminate:(NSNotification *)aNotification {
    (void)aNotification;
}


- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    (void)app;
    return YES;
}


@end

#import "AppDelegate.h"

#import "AppConfigStore.hpp"
#import "AppRuntimePaths.hpp"
#import "../UIConstants.hpp"
#import "../ViewControllers/RootViewController.hpp"

namespace peanutbutter {

NSString *NSStringFromStdString(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()] ?: @"";
}

}  // namespace peanutbutter

static BOOL PBWindowFrameSizeIsValid(NSRect frame) {
    return frame.size.width >= kUIWindowMinWidth &&
           frame.size.width <= kUIWindowMaxWidth &&
           frame.size.height >= kUIWindowMinHeight &&
           frame.size.height <= kUIWindowMaxHeight;
}

static BOOL PBWindowFrameFitsScreenVisibleFrame(NSRect frame, NSScreen *screen) {
    if (screen == nil || !PBWindowFrameSizeIsValid(frame)) {
        return NO;
    }

    NSRect visibleFrame = screen.visibleFrame;
    return frame.size.width <= NSWidth(visibleFrame) &&
           frame.size.height <= NSHeight(visibleFrame);
}

static NSRect PBClampWindowFrameToVisibleFrame(NSRect frame, NSScreen *screen) {
    if (screen == nil) {
        return frame;
    }

    NSRect visibleFrame = screen.visibleFrame;
    CGFloat maxX = NSMaxX(visibleFrame) - NSWidth(frame);
    CGFloat maxY = NSMaxY(visibleFrame) - NSHeight(frame);
    frame.origin.x = MIN(MAX(frame.origin.x, NSMinX(visibleFrame)), maxX);
    frame.origin.y = MIN(MAX(frame.origin.y, NSMinY(visibleFrame)), maxY);
    return frame;
}

static BOOL PBResolveUsableWindowFrame(NSRect frame, NSRect *resolvedFrame) {
    if (!PBWindowFrameSizeIsValid(frame)) {
        return NO;
    }

    NSScreen *bestScreen = nil;
    NSRect bestFrame = NSZeroRect;
    CGFloat bestIntersectionArea = -1.0;

    for (NSScreen *screen in NSScreen.screens) {
        if (!PBWindowFrameFitsScreenVisibleFrame(frame, screen)) {
            continue;
        }

        NSRect clampedFrame = PBClampWindowFrameToVisibleFrame(frame, screen);
        NSRect intersection = NSIntersectionRect(frame, screen.visibleFrame);
        CGFloat intersectionArea = NSWidth(intersection) * NSHeight(intersection);
        if (bestScreen == nil || intersectionArea > bestIntersectionArea) {
            bestScreen = screen;
            bestFrame = clampedFrame;
            bestIntersectionArea = intersectionArea;
        }
    }

    if (bestScreen == nil) {
        NSScreen *fallbackScreen = NSScreen.mainScreen ?: NSScreen.screens.firstObject;
        if (!PBWindowFrameFitsScreenVisibleFrame(frame, fallbackScreen)) {
            return NO;
        }
        bestFrame = PBClampWindowFrameToVisibleFrame(frame, fallbackScreen);
    }

    if (resolvedFrame != nil) {
        *resolvedFrame = bestFrame;
    }
    return YES;
}

@implementation AppDelegate {
    RootViewController *_rootViewController;
    AppConfigStore *_configStore;
    peanutbutter::AppConfigStateV2 _configState;
}

- (NSMenuItem *)menuItemWithTitle:(NSString *)title
                           action:(SEL)action
                    keyEquivalent:(NSString *)keyEquivalent {
    NSMenuItem *item = [[NSMenuItem alloc] initWithTitle:title
                                                  action:action
                                           keyEquivalent:keyEquivalent ?: @""];
    item.target = nil;
    return item;
}

- (NSMenu *)buildApplicationMenu {
    NSString *appName = NSProcessInfo.processInfo.processName ?: @"PeanutButterArchiver";
    NSMenu *menu = [[NSMenu alloc] initWithTitle:appName];

    [menu addItem:[self menuItemWithTitle:[@"About " stringByAppendingString:appName]
                                   action:@selector(orderFrontStandardAboutPanel:)
                            keyEquivalent:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItemWithTitle:[@"Hide " stringByAppendingString:appName]
                                   action:@selector(hide:)
                            keyEquivalent:@"h"]];

    NSMenuItem *hideOthers = [self menuItemWithTitle:@"Hide Others"
                                              action:@selector(hideOtherApplications:)
                                       keyEquivalent:@"h"];
    hideOthers.keyEquivalentModifierMask = NSEventModifierFlagOption | NSEventModifierFlagCommand;
    [menu addItem:hideOthers];
    [menu addItem:[self menuItemWithTitle:@"Show All"
                                   action:@selector(unhideAllApplications:)
                            keyEquivalent:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItemWithTitle:[@"Quit " stringByAppendingString:appName]
                                   action:@selector(terminate:)
                            keyEquivalent:@"q"]];
    return menu;
}

- (NSMenu *)buildEditMenu {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Edit"];
    [menu addItem:[self menuItemWithTitle:@"Undo"
                                   action:NSSelectorFromString(@"undo:")
                            keyEquivalent:@"z"]];
    [menu addItem:[self menuItemWithTitle:@"Redo"
                                   action:NSSelectorFromString(@"redo:")
                            keyEquivalent:@"Z"]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"]];
    [menu addItem:[self menuItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"]];
    [menu addItem:[self menuItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"]];
    [menu addItem:[self menuItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"]];
    return menu;
}

- (NSMenu *)buildWindowMenu {
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Window"];
    [menu addItem:[self menuItemWithTitle:@"Minimize"
                                   action:@selector(performMiniaturize:)
                            keyEquivalent:@"m"]];
    [menu addItem:[self menuItemWithTitle:@"Zoom"
                                   action:@selector(performZoom:)
                            keyEquivalent:@""]];
    [menu addItem:[NSMenuItem separatorItem]];
    [menu addItem:[self menuItemWithTitle:@"Bring All to Front"
                                   action:@selector(arrangeInFront:)
                            keyEquivalent:@""]];
    return menu;
}

- (NSMenu *)buildHelpMenu {
    NSString *appName = NSProcessInfo.processInfo.processName ?: @"PeanutButterArchiver";
    NSMenu *menu = [[NSMenu alloc] initWithTitle:@"Help"];
    NSMenuItem *helpItem = [self menuItemWithTitle:[appName stringByAppendingString:@" Help"]
                                            action:@selector(showHelp:)
                                     keyEquivalent:@"?"];
    [menu addItem:helpItem];
    return menu;
}

- (void)installMainMenuIfNeeded {
    if (NSApp.mainMenu.itemArray.count > 0) {
        return;
    }

    NSMenu *mainMenu = [[NSMenu alloc] initWithTitle:@"Main Menu"];

    NSMenuItem *appRoot = [[NSMenuItem alloc] initWithTitle:@"" action:nil keyEquivalent:@""];
    appRoot.submenu = [self buildApplicationMenu];
    [mainMenu addItem:appRoot];

    NSMenuItem *editRoot = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
    editRoot.submenu = [self buildEditMenu];
    [mainMenu addItem:editRoot];

    NSMenu *windowMenu = [self buildWindowMenu];
    NSMenuItem *windowRoot = [[NSMenuItem alloc] initWithTitle:@"Window" action:nil keyEquivalent:@""];
    windowRoot.submenu = windowMenu;
    [mainMenu addItem:windowRoot];

    NSMenu *helpMenu = [self buildHelpMenu];
    NSMenuItem *helpRoot = [[NSMenuItem alloc] initWithTitle:@"Help" action:nil keyEquivalent:@""];
    helpRoot.submenu = helpMenu;
    [mainMenu addItem:helpRoot];

    [NSApp setMainMenu:mainMenu];
    [NSApp setWindowsMenu:windowMenu];
    [NSApp setHelpMenu:helpMenu];
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
    self.window.delegate = self;
    return self.window;
}

- (BOOL)applySavedWindowFrameIfPossibleToWindow:(NSWindow *)window {
    if (window == nil) {
        return NO;
    }

    NSRect savedFrame = NSMakeRect(_configState.mWindowFrameX,
                                   _configState.mWindowFrameY,
                                   _configState.mWindowFrameWidth,
                                   _configState.mWindowFrameHeight);
    NSRect resolvedFrame = NSZeroRect;
    if (!PBResolveUsableWindowFrame(savedFrame, &resolvedFrame)) {
        return NO;
    }

    [window setFrame:resolvedFrame display:NO];
    return YES;
}

- (void)persistWindowFrame {
    if (_configStore == nil || self.window == nil) {
        return;
    }

    if (!self.window.isVisible || self.window.isMiniaturized) {
        return;
    }

    NSRect frame = self.window.frame;
    if (!PBWindowFrameSizeIsValid(frame)) {
        return;
    }

    _configState.mWindowFrameX = frame.origin.x;
    _configState.mWindowFrameY = frame.origin.y;
    _configState.mWindowFrameWidth = frame.size.width;
    _configState.mWindowFrameHeight = frame.size.height;
    [_configStore saveConfig:_configState error:nil];
}

- (void)applicationWillFinishLaunching:(NSNotification *)notification {
    (void)notification;
    [self installMainMenuIfNeeded];
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

    NSString *inputTextDefault =
        _configState.mTextFieldInputText.empty()
            ? @"input"
            : peanutbutter::NSStringFromStdString(_configState.mTextFieldInputText);
    NSString *archivedTextDefault =
        _configState.mTextFieldArchivedText.empty()
            ? @"archived"
            : peanutbutter::NSStringFromStdString(_configState.mTextFieldArchivedText);
    NSString *unarchivedTextDefault =
        _configState.mTextFieldUnarchivedText.empty()
            ? @"unarchived"
            : peanutbutter::NSStringFromStdString(_configState.mTextFieldUnarchivedText);

    _rootViewController.bundleSourceDefault =
        inputTextDefault;
    _rootViewController.bundleDestinationDefault =
        archivedTextDefault;
    _rootViewController.unbundleSourceDefault =
        archivedTextDefault;
    _rootViewController.unbundleDestinationDefault =
        unarchivedTextDefault;
    _rootViewController.toolsSourceDefault =
        _configState.mTextFieldCompareAText.empty()
            ? (_configState.mTextFieldInputText.empty()
                   ? @"source"
                   : inputTextDefault)
            : peanutbutter::NSStringFromStdString(_configState.mTextFieldCompareAText);
    _rootViewController.toolsDestinationDefault =
        _configState.mTextFieldCompareBText.empty()
            ? (_configState.mTextFieldUnarchivedText.empty()
                   ? @"unarchived"
                   : unarchivedTextDefault)
            : peanutbutter::NSStringFromStdString(_configState.mTextFieldCompareBText);
    _rootViewController.bundleFilePrefixDefault =
        _configState.mBundleFilePrefix.empty()
            ? @"archive"
            : peanutbutter::NSStringFromStdString(_configState.mBundleFilePrefix);
    _rootViewController.bundlePasswordDefault =
        peanutbutter::NSStringFromStdString(_configState.mBundlePassword);
    _rootViewController.unbundlePasswordDefault =
        peanutbutter::NSStringFromStdString(_configState.mUnbundlePassword);
    _rootViewController.unbundleRecoverDefault = _configState.mUnbundleRecover;
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
    if (![self applySavedWindowFrameIfPossibleToWindow:window]) {
        [window center];
    }
    [window makeKeyAndOrderFront:nil];
    [self persistWindowFrame];
    [NSApp activateIgnoringOtherApps:YES];

}


- (void)applicationWillTerminate:(NSNotification *)aNotification {
    (void)aNotification;
    [self persistWindowFrame];
}


- (BOOL)applicationSupportsSecureRestorableState:(NSApplication *)app {
    (void)app;
    return YES;
}

- (void)windowDidMove:(NSNotification *)notification {
    (void)notification;
    [self persistWindowFrame];
}

- (void)windowDidResize:(NSNotification *)notification {
    (void)notification;
    [self persistWindowFrame];
}


@end

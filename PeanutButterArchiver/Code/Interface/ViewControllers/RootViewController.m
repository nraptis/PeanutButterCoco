#import "RootViewController.hpp"

#import "../Application/AppConfigStore.hpp"
#import "../Application/AppShell.hpp"
#import "../Views/HomeToolViewTop.hpp"
#import "../Views/HomeActiveModeContainerView.hpp"
#import "../Views/HomeLogControlView.hpp"
#import "../Views/HomeHeaderView.hpp"
#import "../Views/HomeLogContainerView.hpp"
#import "../Views/HomeLogView.hpp"
#import "../Views/PBDrawableButton.hpp"
#import "HomeContainerViewController.hpp"

static NSString *PBSafeTextValue(NSString *value) {
    return value ?: @"";
}

@implementation RootViewController {
    AppShell *_appShell;
}

- (void)loadView {
    NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 1280.0, 860.0)];
    rootView.translatesAutoresizingMaskIntoConstraints = NO;
    rootView.wantsLayer = YES;
    rootView.layer.backgroundColor = [NSColor colorWithRed:0.065 green:0.065 blue:0.07125 alpha:1.0].CGColor;
    self.view = rootView;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.homeContainerViewController = [[HomeContainerViewController alloc] init];
    [self addChildViewController:self.homeContainerViewController];

    NSView *childView = self.homeContainerViewController.view;
    childView.translatesAutoresizingMaskIntoConstraints = NO;
    [self.view addSubview:childView];

    [NSLayoutConstraint activateConstraints:@[
        [childView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor],
        [childView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor],
        [childView.topAnchor constraintEqualToAnchor:self.view.topAnchor],
        [childView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor],
    ]];

    [self.homeContainerViewController.homeActiveModeContainerView
        applyBundleDefaultsWithSource:self.bundleSourceDefault
                          destination:self.bundleDestinationDefault];
    [self.homeContainerViewController.homeActiveModeContainerView
        applyUnbundleDefaultsWithSource:self.unbundleSourceDefault
                            destination:self.unbundleDestinationDefault];
    [self.homeContainerViewController.homeActiveModeContainerView
        applyToolsDefaultsWithSource:self.toolsSourceDefault
                         destination:self.toolsDestinationDefault];
    [self applyInitialBundleUiState];
    [self wireHomeConfigPersistence];
    self.homeContainerViewController.homeActiveModeContainerView.bundleActionButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.bundleActionButton.action = @selector(handleBundleButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.unbundleActionButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleActionButton.action = @selector(handleBundleButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.toolsActionButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.toolsActionButton.action = @selector(handleBundleButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.unbundleReadManifestButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleReadManifestButton.action = @selector(handleReadManifestButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.bundleSourceBrowseFilesButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.bundleSourceBrowseFilesButton.action = @selector(handleSourceBrowseFilesButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.bundleSourceBrowseButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.bundleSourceBrowseButton.action = @selector(handleSourceBrowseFolderButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.bundleSourceClearButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.bundleSourceClearButton.action = @selector(handleSourceClearButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.bundleDestinationBrowseButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.bundleDestinationBrowseButton.action = @selector(handleDestinationBrowseButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.bundleDestinationClearButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.bundleDestinationClearButton.action = @selector(handleDestinationClearButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.bundleFilePrefixClearButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.bundleFilePrefixClearButton.action = @selector(handleFilePrefixClearButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.unbundleSourceBrowseFilesButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleSourceBrowseFilesButton.action = @selector(handleUnbundleSourceBrowseFilesButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.unbundleSourceBrowseButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleSourceBrowseButton.action = @selector(handleUnbundleSourceBrowseFolderButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.unbundleDestinationBrowseButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleDestinationBrowseButton.action = @selector(handleUnbundleDestinationBrowseButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.unbundleSourceClearButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleSourceClearButton.action = @selector(handleUnbundleSourceClearButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.unbundleDestinationClearButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleDestinationClearButton.action = @selector(handleUnbundleDestinationClearButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.unbundleRecoverCheckbox.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleRecoverCheckbox.action = @selector(handleUnbundleRecoverCheckboxChanged:);
    self.homeContainerViewController.homeActiveModeContainerView.toolsSourceBrowseButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.toolsSourceBrowseButton.action = @selector(handleToolsSourceBrowseButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.toolsDestinationBrowseButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.toolsDestinationBrowseButton.action = @selector(handleToolsDestinationBrowseButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.toolsSourceClearButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.toolsSourceClearButton.action = @selector(handleToolsSourceClearButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.toolsDestinationClearButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.toolsDestinationClearButton.action = @selector(handleToolsDestinationClearButtonPress:);
    self.homeContainerViewController.homeLogControlView.clearLogsButton.target = self;
    self.homeContainerViewController.homeLogControlView.clearLogsButton.action = @selector(handleClearLogsButtonPress:);
    self.homeContainerViewController.homeLogControlView.scrollToBottomButton.target = self;
    self.homeContainerViewController.homeLogControlView.scrollToBottomButton.action = @selector(handleScrollToBottomButtonPress:);
    self.homeContainerViewController.homeLogControlView.verboseEventsButton.target = self;
    self.homeContainerViewController.homeLogControlView.verboseEventsButton.action = @selector(handleVerboseEventsCheckboxChanged:);
    self.homeContainerViewController.homeActiveModeContainerView.progressCancelButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.progressCancelButton.action = @selector(handleCancelButtonPress:);
    __weak typeof(self) weakSelf = self;
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    [activeView setBundleSourcePathDropHandler:^(NSString *path) {
        [weakSelf applyBundleInputValue:path origin:nil];
        [weakSelf persistHomeUiState];
    }];
    [activeView setBundleDestinationPathDropHandler:^(NSString *path) {
        [weakSelf applyArchivedValue:path origin:nil];
        [weakSelf persistHomeUiState];
    }];
    [activeView setUnbundleSourcePathDropHandler:^(NSString *path) {
        [weakSelf applyArchivedValue:path origin:nil];
        [weakSelf persistHomeUiState];
    }];
    [activeView setUnbundleDestinationPathDropHandler:^(NSString *path) {
        [weakSelf applyUnarchivedValue:path origin:nil];
        [weakSelf persistHomeUiState];
    }];
    [activeView setToolsSourcePathDropHandler:^(NSString *path) {
        [weakSelf applyToolsCompareAValue:path origin:nil];
        [weakSelf persistHomeUiState];
    }];
    [activeView setToolsDestinationPathDropHandler:^(NSString *path) {
        [weakSelf applyToolsCompareBValue:path origin:nil];
        [weakSelf persistHomeUiState];
    }];

    _appShell = [[AppShell alloc] initWithHomeContainerViewController:self.homeContainerViewController];
    [_appShell setVerboseRuntimeEventsEnabled:
        self.homeContainerViewController.homeLogControlView.verboseEventsButton.isSelected];
    [_appShell startPolling];
}

- (void)handleBundleButtonPress:(id)sender {
    (void)sender;
    if (![self.homeContainerViewController canStartPrimaryAction]) {
        return;
    }

    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    NSInteger modeIndex = self.homeContainerViewController.homeToolViewTop.modeToggle.selectedSegment;
    NSString *actionTitle = @"Bundle";
    switch (modeIndex) {
        case 1: actionTitle = @"Unbundle"; break;
        case 2: actionTitle = @"Tools"; break;
        case 0:
        default: break;
    }
    [self.homeContainerViewController transitionToGhostStateWithTitle:actionTitle
                                                               detail:@"Preparing..."];

    switch (modeIndex) {
        case 1:
            [_appShell enqueueUnbundleRequestWithSourcePath:activeView.unbundleSourceTextField.stringValue
                                       destinationDirectory:activeView.unbundleDestinationTextField.stringValue
                                              recoverEnabled:(activeView.unbundleRecoverCheckbox.state == NSControlStateValueOn)
                                                   password:activeView.unbundlePasswordTextField.stringValue];
            break;
        case 2:
            [_appShell enqueueSanityRequestWithLeftDirectory:activeView.toolsSourceTextField.stringValue
                                              rightDirectory:activeView.toolsDestinationTextField.stringValue
                                                ignoreHidden:(activeView.toolsIgnoreHiddenCheckbox.state == NSControlStateValueOn)];
            break;
        case 0:
        default:
            [_appShell enqueueBundleRequestWithSourceDirectory:activeView.bundleSourceTextField.stringValue
                                          destinationDirectory:activeView.bundleDestinationTextField.stringValue
                                                    filePrefix:activeView.bundleFilePrefixTextField.stringValue
                                                 repairEnabled:(activeView.bundleRepairCheckbox.state == NSControlStateValueOn)
                                                   safeEnabled:YES
                                             encryptionEnabled:(activeView.bundleEncryptCheckbox.state == NSControlStateValueOn)
                                           includePreviewEnabled:(activeView.bundleIncludePreviewCheckbox.state == NSControlStateValueOn)
                                                      password:activeView.bundlePasswordTextField.stringValue
                                                repairSizeTitle:activeView.bundleRepairSizeCombo.titleOfSelectedItem
                                                blockCountTitle:activeView.bundleBlockCountCombo.titleOfSelectedItem
                                       encryptionStrengthTitle:activeView.bundleEncryptionStrengthCombo.titleOfSelectedItem
                                             tableStrengthTitle:activeView.bundleTableStrengthCombo.titleOfSelectedItem];
            break;
    }
}

- (void)handleReadManifestButtonPress:(id)sender {
    (void)sender;
    if (![self.homeContainerViewController canStartPrimaryAction]) {
        return;
    }

    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    [self.homeContainerViewController transitionToGhostStateWithTitle:@"Read Manifest"
                                                               detail:@"Preparing..."];
    [_appShell enqueueManifestRequestWithSourcePath:activeView.unbundleSourceTextField.stringValue
                               destinationDirectory:activeView.unbundleDestinationTextField.stringValue
                                           password:activeView.unbundlePasswordTextField.stringValue];
}

- (void)handleClearLogsButtonPress:(id)sender {
    (void)sender;
    [self.homeContainerViewController.homeLogContainerView.homeLogView clearAll];
}

- (void)handleScrollToBottomButtonPress:(id)sender {
    (void)sender;
    [self.homeContainerViewController.homeLogContainerView.homeLogView scrollToBottom];
}

- (void)handleVerboseEventsCheckboxChanged:(id)sender {
    PBDrawableButton *button = (PBDrawableButton *)sender;
    [_appShell setVerboseRuntimeEventsEnabled:button.isSelected];
}

- (void)handleCancelButtonPress:(id)sender {
    (void)sender;
    [_appShell enqueueCancelRequest];
}

- (void)handleSourceBrowseFilesButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseSourceFileWithTitle:@"Choose Source File"];
    if (selectedURL != nil) {
        [self applyBundleInputValue:selectedURL.path origin:nil];
        [self persistHomeUiState];
    }
}

- (void)handleSourceBrowseFolderButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Source Folder"];
    if (selectedURL != nil) {
        [self applyBundleInputValue:selectedURL.path origin:nil];
        [self persistHomeUiState];
    }
}

- (void)handleDestinationBrowseButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Destination Directory"];
    if (selectedURL != nil) {
        [self applyArchivedValue:selectedURL.path origin:nil];
        [self persistHomeUiState];
    }
}

- (void)handleUnbundleSourceBrowseFilesButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseSourceFileWithTitle:@"Choose Source File"];
    if (selectedURL != nil) {
        [self applyArchivedValue:selectedURL.path origin:nil];
        [self persistHomeUiState];
    }
}

- (void)handleUnbundleSourceBrowseFolderButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Source Folder"];
    if (selectedURL != nil) {
        [self applyArchivedValue:selectedURL.path origin:nil];
        [self persistHomeUiState];
    }
}

- (void)handleUnbundleDestinationBrowseButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Destination Directory"];
    if (selectedURL != nil) {
        [self applyUnarchivedValue:selectedURL.path origin:nil];
        [self persistHomeUiState];
    }
}

- (void)handleSourceClearButtonPress:(id)sender {
    (void)sender;
    [self applyBundleInputValue:@"" origin:nil];
    [self persistHomeUiState];
}

- (void)handleDestinationClearButtonPress:(id)sender {
    (void)sender;
    [self applyArchivedValue:@"" origin:nil];
    [self persistHomeUiState];
}

- (void)handleFilePrefixClearButtonPress:(id)sender {
    (void)sender;
    self.homeContainerViewController.homeActiveModeContainerView.bundleFilePrefixTextField.stringValue = @"";
    [self persistHomeUiState];
}

- (void)handleUnbundleSourceClearButtonPress:(id)sender {
    (void)sender;
    [self applyArchivedValue:@"" origin:nil];
    [self persistHomeUiState];
}

- (void)handleUnbundleDestinationClearButtonPress:(id)sender {
    (void)sender;
    [self applyUnarchivedValue:@"" origin:nil];
    [self persistHomeUiState];
}

- (void)handleToolsSourceBrowseButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Source Folder"];
    if (selectedURL != nil) {
        [self applyToolsCompareAValue:selectedURL.path origin:nil];
        [self persistHomeUiState];
    }
}

- (void)handleToolsDestinationBrowseButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Unarchived Folder"];
    if (selectedURL != nil) {
        [self applyToolsCompareBValue:selectedURL.path origin:nil];
        [self persistHomeUiState];
    }
}

- (void)handleToolsSourceClearButtonPress:(id)sender {
    (void)sender;
    [self applyToolsCompareAValue:@"" origin:nil];
    [self persistHomeUiState];
}

- (void)handleToolsDestinationClearButtonPress:(id)sender {
    (void)sender;
    [self applyToolsCompareBValue:@"" origin:nil];
    [self persistHomeUiState];
}

- (void)handleUnbundleRecoverCheckboxChanged:(id)sender {
    (void)sender;
    [self persistHomeUiState];
}

- (void)controlTextDidChange:(NSNotification *)notification {
    NSTextField *textField = notification.object;
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;

    if (textField == activeView.bundleSourceTextField) {
        [self applyBundleInputValue:textField.stringValue origin:textField];
    } else if (textField == activeView.bundleDestinationTextField ||
               textField == activeView.unbundleSourceTextField) {
        [self applyArchivedValue:textField.stringValue origin:textField];
    } else if (textField == activeView.unbundleDestinationTextField) {
        [self applyUnarchivedValue:textField.stringValue origin:textField];
    } else if (textField == activeView.toolsSourceTextField) {
        [self applyToolsCompareAValue:textField.stringValue origin:textField];
    } else if (textField == activeView.toolsDestinationTextField) {
        [self applyToolsCompareBValue:textField.stringValue origin:textField];
    }

    [self persistHomeUiState];
}

- (void)handleBundleConfigControlChanged:(id)sender {
    (void)sender;
    [self persistHomeUiState];
}

- (NSURL *)chooseDirectoryWithTitle:(NSString *)title {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = NO;
    panel.canChooseDirectories = YES;
    panel.allowsMultipleSelection = NO;
    panel.canCreateDirectories = YES;
    panel.title = title ?: @"Choose Directory";
    return ([panel runModal] == NSModalResponseOK) ? panel.URL : nil;
}

- (NSURL *)chooseSourceFileWithTitle:(NSString *)title {
    NSOpenPanel *panel = [NSOpenPanel openPanel];
    panel.canChooseFiles = YES;
    panel.canChooseDirectories = NO;
    panel.allowsMultipleSelection = NO;
    panel.canCreateDirectories = NO;
    panel.title = title ?: @"Choose File";
    return ([panel runModal] == NSModalResponseOK) ? panel.URL : nil;
}

- (void)applyInitialBundleUiState {
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    activeView.bundleFilePrefixTextField.stringValue =
        (self.bundleFilePrefixDefault.length > 0) ? self.bundleFilePrefixDefault : @"archive";
    activeView.bundlePasswordTextField.stringValue = self.bundlePasswordDefault ?: @"";
    activeView.unbundlePasswordTextField.stringValue = self.unbundlePasswordDefault ?: @"";
    [activeView applyUnbundleRecoverDefaultEnabled:self.unbundleRecoverDefault];
    activeView.bundleRepairCheckbox.state = self.bundleRepairDefault ? NSControlStateValueOn : NSControlStateValueOff;
    activeView.bundleEncryptCheckbox.state = self.bundleEncryptDefault ? NSControlStateValueOn : NSControlStateValueOff;
    activeView.bundleIncludePreviewCheckbox.state = self.bundleIncludePreviewDefault ? NSControlStateValueOn : NSControlStateValueOff;

    NSString *blockCountTitle =
        (self.bundleBlockCountDefault.length > 0) ? self.bundleBlockCountDefault : @"5 blocks";
    if ([activeView.bundleBlockCountCombo indexOfItemWithTitle:blockCountTitle] < 0) {
        blockCountTitle = @"5 blocks";
    }
    [activeView.bundleBlockCountCombo selectItemWithTitle:blockCountTitle];
    [activeView.bundleEncryptionStrengthCombo selectItemWithTitle:(self.bundleEncryptionStrengthDefault.length > 0 ? self.bundleEncryptionStrengthDefault : @"Encryption: High")];
    [activeView.bundleTableStrengthCombo selectItemWithTitle:(self.bundleTableStrengthDefault.length > 0 ? self.bundleTableStrengthDefault : @"Tables: High")];
    NSString *repairPercentTitle =
        (self.bundleRepairSizeDefault.length > 0 && [self.bundleRepairSizeDefault containsString:@"%"])
            ? self.bundleRepairSizeDefault
            : @"20%";
    if ([activeView.bundleRepairSizeCombo indexOfItemWithTitle:repairPercentTitle] < 0) {
        repairPercentTitle = @"20%";
    }
    [activeView.bundleRepairSizeCombo selectItemWithTitle:repairPercentTitle];

    NSInteger safeHomeTab = self.homeTabDefault;
    if (safeHomeTab < 0 || safeHomeTab >= self.homeContainerViewController.homeToolViewTop.modeToggle.segmentCount) {
        safeHomeTab = 0;
    }
    self.homeContainerViewController.homeToolViewTop.modeToggle.selectedSegment = safeHomeTab;
    activeView.activeHomeTabIndex = safeHomeTab;
}

- (void)wireHomeConfigPersistence {
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    activeView.bundleSourceTextField.delegate = self;
    activeView.bundleDestinationTextField.delegate = self;
    activeView.bundleFilePrefixTextField.delegate = self;
    activeView.bundlePasswordTextField.delegate = self;
    activeView.unbundleSourceTextField.delegate = self;
    activeView.unbundleDestinationTextField.delegate = self;
    activeView.unbundlePasswordTextField.delegate = self;
    activeView.toolsSourceTextField.delegate = self;
    activeView.toolsDestinationTextField.delegate = self;

    for (NSControl *control in @[
            activeView.bundleRepairCheckbox,
            activeView.bundleEncryptCheckbox,
            activeView.bundleIncludePreviewCheckbox,
            activeView.bundleRepairSizeCombo,
            activeView.bundleEncryptionStrengthCombo,
            activeView.bundleTableStrengthCombo,
            activeView.bundleBlockCountCombo,
            self.homeContainerViewController.homeToolViewTop.modeToggle
         ]) {
        control.target = self;
        control.action = @selector(handleBundleConfigControlChanged:);
    }
}

- (void)applyBundleInputValue:(NSString *)value
                       origin:(NSTextField *)originField {
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    NSString *safeValue = PBSafeTextValue(value);
    if (originField != activeView.bundleSourceTextField) {
        activeView.bundleSourceTextField.stringValue = safeValue;
    }
    activeView.toolsSourceTextField.stringValue = safeValue;
}

- (void)applyArchivedValue:(NSString *)value
                    origin:(NSTextField *)originField {
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    NSString *safeValue = PBSafeTextValue(value);
    if (originField != activeView.bundleDestinationTextField) {
        activeView.bundleDestinationTextField.stringValue = safeValue;
    }
    if (originField != activeView.unbundleSourceTextField) {
        activeView.unbundleSourceTextField.stringValue = safeValue;
    }
}

- (void)applyUnarchivedValue:(NSString *)value
                      origin:(NSTextField *)originField {
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    NSString *safeValue = PBSafeTextValue(value);
    if (originField != activeView.unbundleDestinationTextField) {
        activeView.unbundleDestinationTextField.stringValue = safeValue;
    }
    activeView.toolsDestinationTextField.stringValue = safeValue;
}

- (void)applyToolsCompareAValue:(NSString *)value
                         origin:(NSTextField *)originField {
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    if (originField != activeView.toolsSourceTextField) {
        activeView.toolsSourceTextField.stringValue = PBSafeTextValue(value);
    }
}

- (void)applyToolsCompareBValue:(NSString *)value
                         origin:(NSTextField *)originField {
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    if (originField != activeView.toolsDestinationTextField) {
        activeView.toolsDestinationTextField.stringValue = PBSafeTextValue(value);
    }
}

- (void)persistHomeUiState {
    if (self.configStore == nil) {
        return;
    }

    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    NSInteger homeTab = self.homeContainerViewController.homeToolViewTop.modeToggle.selectedSegment;
    NSError *error = nil;
    activeView.activeHomeTabIndex = homeTab;
    [self.configStore saveHomeUiStateWithHomeTab:homeTab
                                       inputText:activeView.bundleSourceTextField.stringValue
                                    archivedText:activeView.bundleDestinationTextField.stringValue
                                  unarchivedText:activeView.unbundleDestinationTextField.stringValue
                                    compareAText:activeView.toolsSourceTextField.stringValue
                                    compareBText:activeView.toolsDestinationTextField.stringValue
                                      filePrefix:activeView.bundleFilePrefixTextField.stringValue
                                   repairEnabled:(activeView.bundleRepairCheckbox.state == NSControlStateValueOn)
                                     safeEnabled:YES
                               encryptionEnabled:(activeView.bundleEncryptCheckbox.state == NSControlStateValueOn)
                           includePreviewEnabled:(activeView.bundleIncludePreviewCheckbox.state == NSControlStateValueOn)
                                 blockCountTitle:activeView.bundleBlockCountCombo.titleOfSelectedItem
                        encryptionStrengthTitle:activeView.bundleEncryptionStrengthCombo.titleOfSelectedItem
                              tableStrengthTitle:activeView.bundleTableStrengthCombo.titleOfSelectedItem
                               repairSizeTitle:activeView.bundleRepairSizeCombo.titleOfSelectedItem
                                       password:activeView.bundlePasswordTextField.stringValue
                                recoverEnabled:(activeView.unbundleRecoverCheckbox.state == NSControlStateValueOn)
                             unbundlePassword:activeView.unbundlePasswordTextField.stringValue
                                          error:&error];
    (void)error;
}

@end

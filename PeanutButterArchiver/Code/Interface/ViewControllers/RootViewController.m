#import "RootViewController.hpp"

#import "../Application/AppConfigStore.hpp"
#import "../Application/AppShell.hpp"
#import "../Views/HomeToolViewTop.hpp"
#import "../Views/HomeActiveModeContainerView.hpp"
#import "../Views/HomeHeaderView.hpp"
#import "../Views/HomeLogContainerView.hpp"
#import "../Views/HomeLogView.hpp"
#import "../Views/HomeToolViewSplitter.hpp"
#import "HomeContainerViewController.hpp"

@implementation RootViewController {
    AppShell *_appShell;
}

- (void)loadView {
    NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, 1280.0, 860.0)];
    rootView.translatesAutoresizingMaskIntoConstraints = NO;
    rootView.wantsLayer = YES;
    rootView.layer.backgroundColor = NSColor.windowBackgroundColor.CGColor;
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
    [self applyInitialBundleUiState];
    [self wireBundleConfigPersistence];
    self.homeContainerViewController.homeActiveModeContainerView.bundleActionButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.bundleActionButton.action = @selector(handleBundleButtonPress:);
    self.homeContainerViewController.homeActiveModeContainerView.unbundleActionButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleActionButton.action = @selector(handleBundleButtonPress:);
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
    self.homeContainerViewController.homeToolViewSplitter.clearLogsButton.target = self;
    self.homeContainerViewController.homeToolViewSplitter.clearLogsButton.action = @selector(handleClearLogsButtonPress:);
    self.homeContainerViewController.homeToolViewSplitter.scrollToBottomButton.target = self;
    self.homeContainerViewController.homeToolViewSplitter.scrollToBottomButton.action = @selector(handleScrollToBottomButtonPress:);
    self.homeContainerViewController.homeToolViewSplitter.verboseEventsCheckbox.target = self;
    self.homeContainerViewController.homeToolViewSplitter.verboseEventsCheckbox.action = @selector(handleVerboseEventsCheckboxChanged:);
    self.homeContainerViewController.homeActiveModeContainerView.progressCancelButton.target = self;
    self.homeContainerViewController.homeActiveModeContainerView.progressCancelButton.action = @selector(handleCancelButtonPress:);
    __weak typeof(self) weakSelf = self;
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    [activeView setBundleSourcePathDropHandler:^(NSString *path) {
        weakSelf.homeContainerViewController.homeActiveModeContainerView.bundleSourceTextField.stringValue = path ?: @"";
        [weakSelf persistBundleUiState];
    }];
    [activeView setBundleDestinationPathDropHandler:^(NSString *path) {
        weakSelf.homeContainerViewController.homeActiveModeContainerView.bundleDestinationTextField.stringValue = path ?: @"";
        [weakSelf persistBundleUiState];
    }];
    [activeView setUnbundleSourcePathDropHandler:^(NSString *path) {
        weakSelf.homeContainerViewController.homeActiveModeContainerView.unbundleSourceTextField.stringValue = path ?: @"";
        [weakSelf persistBundleUiState];
    }];
    [activeView setUnbundleDestinationPathDropHandler:^(NSString *path) {
        weakSelf.homeContainerViewController.homeActiveModeContainerView.unbundleDestinationTextField.stringValue = path ?: @"";
        [weakSelf persistBundleUiState];
    }];

    _appShell = [[AppShell alloc] initWithHomeContainerViewController:self.homeContainerViewController];
    [_appShell setVerboseRuntimeEventsEnabled:
        (self.homeContainerViewController.homeToolViewSplitter.verboseEventsCheckbox.state == NSControlStateValueOn)];
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
        case 2: actionTitle = @"Repair"; break;
        case 3: actionTitle = @"Folder Compare"; break;
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
            [_appShell enqueueRepairRequestWithSourcePath:activeView.unbundleSourceTextField.stringValue
                                     destinationDirectory:activeView.unbundleDestinationTextField.stringValue
                                         aggressiveEnabled:(activeView.unbundleRecoverCheckbox.state == NSControlStateValueOn)
                                                 password:activeView.unbundlePasswordTextField.stringValue];
            break;
        case 3:
            [_appShell enqueueSanityRequestWithLeftDirectory:activeView.unbundleSourceTextField.stringValue
                                              rightDirectory:activeView.unbundleDestinationTextField.stringValue
                                                ignoreHidden:(activeView.unbundleRecoverCheckbox.state == NSControlStateValueOn)];
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
    NSButton *checkBox = (NSButton *)sender;
    [_appShell setVerboseRuntimeEventsEnabled:(checkBox.state == NSControlStateValueOn)];
}

- (void)handleCancelButtonPress:(id)sender {
    (void)sender;
    [_appShell enqueueCancelRequest];
}

- (void)handleSourceBrowseFilesButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseSourceFileWithTitle:@"Choose Source File"];
    if (selectedURL != nil) {
        self.homeContainerViewController.homeActiveModeContainerView.bundleSourceTextField.stringValue = selectedURL.path ?: @"";
        [self persistBundleUiState];
    }
}

- (void)handleSourceBrowseFolderButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Source Folder"];
    if (selectedURL != nil) {
        self.homeContainerViewController.homeActiveModeContainerView.bundleSourceTextField.stringValue = selectedURL.path ?: @"";
        [self persistBundleUiState];
    }
}

- (void)handleDestinationBrowseButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Destination Directory"];
    if (selectedURL != nil) {
        self.homeContainerViewController.homeActiveModeContainerView.bundleDestinationTextField.stringValue = selectedURL.path ?: @"";
        [self persistBundleUiState];
    }
}

- (void)handleUnbundleSourceBrowseFilesButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseSourceFileWithTitle:@"Choose Source File"];
    if (selectedURL != nil) {
        self.homeContainerViewController.homeActiveModeContainerView.unbundleSourceTextField.stringValue = selectedURL.path ?: @"";
        [self persistBundleUiState];
    }
}

- (void)handleUnbundleSourceBrowseFolderButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Source Folder"];
    if (selectedURL != nil) {
        self.homeContainerViewController.homeActiveModeContainerView.unbundleSourceTextField.stringValue = selectedURL.path ?: @"";
        [self persistBundleUiState];
    }
}

- (void)handleUnbundleDestinationBrowseButtonPress:(id)sender {
    (void)sender;
    NSURL *selectedURL = [self chooseDirectoryWithTitle:@"Choose Destination Directory"];
    if (selectedURL != nil) {
        self.homeContainerViewController.homeActiveModeContainerView.unbundleDestinationTextField.stringValue = selectedURL.path ?: @"";
        [self persistBundleUiState];
    }
}

- (void)handleSourceClearButtonPress:(id)sender {
    (void)sender;
    self.homeContainerViewController.homeActiveModeContainerView.bundleSourceTextField.stringValue = @"";
    [self persistBundleUiState];
}

- (void)handleDestinationClearButtonPress:(id)sender {
    (void)sender;
    self.homeContainerViewController.homeActiveModeContainerView.bundleDestinationTextField.stringValue = @"";
    [self persistBundleUiState];
}

- (void)handleFilePrefixClearButtonPress:(id)sender {
    (void)sender;
    self.homeContainerViewController.homeActiveModeContainerView.bundleFilePrefixTextField.stringValue = @"";
    [self persistBundleUiState];
}

- (void)handleUnbundleSourceClearButtonPress:(id)sender {
    (void)sender;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleSourceTextField.stringValue = @"";
    [self persistBundleUiState];
}

- (void)handleUnbundleDestinationClearButtonPress:(id)sender {
    (void)sender;
    self.homeContainerViewController.homeActiveModeContainerView.unbundleDestinationTextField.stringValue = @"";
    [self persistBundleUiState];
}

- (void)handleUnbundleRecoverCheckboxChanged:(id)sender {
    (void)sender;
    [self persistBundleUiState];
}

- (void)controlTextDidChange:(NSNotification *)notification {
    (void)notification;
    [self persistBundleUiState];
}

- (void)handleBundleConfigControlChanged:(id)sender {
    (void)sender;
    [self persistBundleUiState];
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

- (void)wireBundleConfigPersistence {
    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    activeView.bundleSourceTextField.delegate = self;
    activeView.bundleDestinationTextField.delegate = self;
    activeView.bundleFilePrefixTextField.delegate = self;
    activeView.bundlePasswordTextField.delegate = self;
    activeView.unbundleSourceTextField.delegate = self;
    activeView.unbundleDestinationTextField.delegate = self;
    activeView.unbundlePasswordTextField.delegate = self;

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

- (void)persistBundleUiState {
    if (self.configStore == nil) {
        return;
    }

    HomeActiveModeContainerView *activeView = self.homeContainerViewController.homeActiveModeContainerView;
    NSInteger homeTab = self.homeContainerViewController.homeToolViewTop.modeToggle.selectedSegment;
    NSError *error = nil;
    activeView.activeHomeTabIndex = homeTab;
    if (homeTab == 0) {
        [self.configStore saveBundleUiStateWithHomeTab:homeTab
                                                source:activeView.bundleSourceTextField.stringValue
                                           destination:activeView.bundleDestinationTextField.stringValue
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
                                                  error:&error];
    } else {
        [self.configStore saveUnbundleUiStateWithHomeTab:homeTab
                                                  source:activeView.unbundleSourceTextField.stringValue
                                             destination:activeView.unbundleDestinationTextField.stringValue
                                          recoverEnabled:(activeView.unbundleRecoverCheckbox.state == NSControlStateValueOn)
                                                password:activeView.unbundlePasswordTextField.stringValue
                                                   error:&error];
    }
    (void)error;
}

@end

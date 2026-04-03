#import "HomeActiveModeContainerView.hpp"

#import "../UIConstants.hpp"
#import "MainActionButton.hpp"
#import "ProgressPanelView.hpp"
#import "ToolBarButtonChunkView.hpp"
#import "ToolBarCheckBoxChunkView.hpp"
#import "ToolBarComboBoxChunkView.hpp"
#import "ToolBarTextFieldChunkView.hpp"
#import "ToolPanelView.hpp"

static NSStackView *MakeRowStackView(NSArray<NSView *> *chunks) {
    NSStackView *stackView = [[NSStackView alloc] initWithFrame:NSZeroRect];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    stackView.alignment = NSLayoutAttributeCenterY;
    stackView.spacing = kUIToolBarChunkSpacing;
    stackView.distribution = NSStackViewDistributionFill;
    for (NSView *chunk in chunks) {
        [stackView addArrangedSubview:chunk];
    }
    return stackView;
}

static NSStackView *MakeEqualWidthStackView(NSArray<NSView *> *chunks) {
    NSStackView *stackView = [[NSStackView alloc] initWithFrame:NSZeroRect];
    stackView.translatesAutoresizingMaskIntoConstraints = NO;
    stackView.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    stackView.alignment = NSLayoutAttributeCenterY;
    stackView.spacing = kUIItemSpacingH;
    stackView.distribution = NSStackViewDistributionFillEqually;
    for (NSView *chunk in chunks) {
        [stackView addArrangedSubview:chunk];
    }
    return stackView;
}

static NSView *MakeRightSectionContainer(NSView *contentView) {
    NSView *container = [[NSView alloc] initWithFrame:NSZeroRect];
    container.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:contentView];
    [NSLayoutConstraint activateConstraints:@[
        [contentView.leadingAnchor constraintEqualToAnchor:container.leadingAnchor],
        [contentView.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
        [contentView.topAnchor constraintEqualToAnchor:container.topAnchor],
        [contentView.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
    ]];
    return container;
}

static NSView *MakeTrailingAlignedRightSection(NSView *contentView,
                                               CGFloat contentWidth) {
    NSView *container = [[NSView alloc] initWithFrame:NSZeroRect];
    container.translatesAutoresizingMaskIntoConstraints = NO;
    [container addSubview:contentView];
    [NSLayoutConstraint activateConstraints:@[
        [contentView.trailingAnchor constraintEqualToAnchor:container.trailingAnchor],
        [contentView.topAnchor constraintEqualToAnchor:container.topAnchor],
        [contentView.bottomAnchor constraintEqualToAnchor:container.bottomAnchor],
        [contentView.widthAnchor constraintEqualToConstant:contentWidth],
    ]];
    return container;
}

static NSView *MakeTextInputRow(NSView *clearChunk,
                                NSView *textChunk,
                                NSView *rightSectionView,
                                CGFloat rightSectionWidth) {
    NSView *row = [[NSView alloc] initWithFrame:NSZeroRect];
    row.translatesAutoresizingMaskIntoConstraints = NO;
    [row addSubview:clearChunk];
    [row addSubview:textChunk];
    [row addSubview:rightSectionView];

    [NSLayoutConstraint activateConstraints:@[
        [clearChunk.leadingAnchor constraintEqualToAnchor:row.leadingAnchor],
        [clearChunk.topAnchor constraintEqualToAnchor:row.topAnchor],
        [clearChunk.bottomAnchor constraintEqualToAnchor:row.bottomAnchor],
        [clearChunk.widthAnchor constraintEqualToConstant:kUIClearButtonWidth],

        [textChunk.leadingAnchor constraintEqualToAnchor:clearChunk.trailingAnchor constant:kUIItemSpacingH],
        [textChunk.topAnchor constraintEqualToAnchor:row.topAnchor],
        [textChunk.bottomAnchor constraintEqualToAnchor:row.bottomAnchor],

        [rightSectionView.leadingAnchor constraintEqualToAnchor:textChunk.trailingAnchor constant:kUIItemSpacingH],
        [rightSectionView.trailingAnchor constraintEqualToAnchor:row.trailingAnchor],
        [rightSectionView.topAnchor constraintEqualToAnchor:row.topAnchor],
        [rightSectionView.bottomAnchor constraintEqualToAnchor:row.bottomAnchor],
        [rightSectionView.widthAnchor constraintEqualToConstant:rightSectionWidth],
    ]];
    return row;
}

static NSView *MakeBlankRow(void) {
    NSView *row = [[NSView alloc] initWithFrame:NSZeroRect];
    row.translatesAutoresizingMaskIntoConstraints = NO;
    return row;
}

static NSView *MakeSingleFillRow(NSView *chunk) {
    NSView *row = [[NSView alloc] initWithFrame:NSZeroRect];
    row.translatesAutoresizingMaskIntoConstraints = NO;
    [row addSubview:chunk];
    [NSLayoutConstraint activateConstraints:@[
        [chunk.leadingAnchor constraintEqualToAnchor:row.leadingAnchor],
        [chunk.trailingAnchor constraintEqualToAnchor:row.trailingAnchor],
        [chunk.topAnchor constraintEqualToAnchor:row.topAnchor],
        [chunk.bottomAnchor constraintEqualToAnchor:row.bottomAnchor],
    ]];
    return row;
}

static NSView *MakePrefixAndBlockCountRow(NSView *prefixChunk,
                                          NSView *clearChunk,
                                          NSView *blockCountChunk) {
    return MakeTextInputRow(clearChunk,
                            prefixChunk,
                            MakeRightSectionContainer(blockCountChunk),
                            kUIMenuSectionRightWidth);
}

static NSView *MakeEncryptionControlsRow(NSView *checkChunk,
                                         NSView *encryptionChunk,
                                         NSView *tableChunk) {
    NSView *row = [[NSView alloc] initWithFrame:NSZeroRect];
    NSStackView *comboStack = MakeEqualWidthStackView(@[encryptionChunk, tableChunk]);
    row.translatesAutoresizingMaskIntoConstraints = NO;
    [row addSubview:checkChunk];
    [row addSubview:comboStack];

    [NSLayoutConstraint activateConstraints:@[
        [checkChunk.leadingAnchor constraintEqualToAnchor:row.leadingAnchor],
        [checkChunk.topAnchor constraintEqualToAnchor:row.topAnchor],
        [checkChunk.bottomAnchor constraintEqualToAnchor:row.bottomAnchor],

        [comboStack.leadingAnchor constraintEqualToAnchor:checkChunk.trailingAnchor constant:kUIItemSpacingH],
        [comboStack.trailingAnchor constraintEqualToAnchor:row.trailingAnchor],
        [comboStack.topAnchor constraintEqualToAnchor:row.topAnchor],
        [comboStack.bottomAnchor constraintEqualToAnchor:row.bottomAnchor],
    ]];
    return row;
}

static CGFloat RequiredBundlePanelHeight(void) {
    return kUIActiveZonePaddingTop
        + [ToolPanelView requiredHeightForRowCount:2]
        + kUIPanelSpacingV
        + [ToolPanelView requiredHeightForRowCount:1]
        + kUIPanelSpacingV
        + [ToolPanelView requiredHeightForRowCount:2];
}

static CGFloat RequiredUnbundlePanelHeight(void) {
    return kUIActiveZonePaddingTop
        + [ToolPanelView requiredHeightForRowCount:2]
        + kUIPanelSpacingV
        + [ToolPanelView requiredHeightForRowCount:2];
}

@implementation HomeActiveModeContainerView {
    NSView *_bundlePanelView;
    NSView *_unbundlePanelView;
    ProgressPanelView *_progressPanelView;
    ToolBarTextFieldChunkView *_bundleSourceChunkView;
    ToolBarTextFieldChunkView *_bundleDestinationChunkView;
    ToolBarTextFieldChunkView *_unbundleSourceChunkView;
    ToolBarTextFieldChunkView *_unbundleDestinationChunkView;
    ToolBarButtonChunkView *_unbundleSourceBrowseFilesChunkView;
    ToolBarButtonChunkView *_unbundleSourceBrowseFolderChunkView;
    ToolBarCheckBoxRightAlignedChunkView *_unbundleRecoverChunkView;
    MainActionButton *_unbundleActionView;
    BOOL _unbundleRecoverEnabled;
    BOOL _repairAggressiveEnabled;
    BOOL _folderCompareIgnoreHiddenEnabled;
}

@synthesize bundleSourceTextField = _bundleSourceTextField;
@synthesize bundleDestinationTextField = _bundleDestinationTextField;
@synthesize bundleFilePrefixTextField = _bundleFilePrefixTextField;
@synthesize bundleSourceClearButton = _bundleSourceClearButton;
@synthesize bundleDestinationClearButton = _bundleDestinationClearButton;
@synthesize bundleFilePrefixClearButton = _bundleFilePrefixClearButton;
@synthesize bundleSourceBrowseFilesButton = _bundleSourceBrowseFilesButton;
@synthesize bundleSourceBrowseButton = _bundleSourceBrowseButton;
@synthesize bundleDestinationBrowseButton = _bundleDestinationBrowseButton;
@synthesize bundleRepairCheckbox = _bundleRepairCheckbox;
@synthesize bundleIncludePreviewCheckbox = _bundleIncludePreviewCheckbox;
@synthesize bundleRepairSizeCombo = _bundleRepairSizeCombo;
@synthesize bundleEncryptCheckbox = _bundleEncryptCheckbox;
@synthesize bundleEncryptionStrengthCombo = _bundleEncryptionStrengthCombo;
@synthesize bundleTableStrengthCombo = _bundleTableStrengthCombo;
@synthesize bundlePasswordTextField = _bundlePasswordTextField;
@synthesize bundleBlockCountCombo = _bundleBlockCountCombo;
@synthesize bundleActionButton = _bundleActionButton;
@synthesize unbundleSourceTextField = _unbundleSourceTextField;
@synthesize unbundleDestinationTextField = _unbundleDestinationTextField;
@synthesize unbundlePasswordTextField = _unbundlePasswordTextField;
@synthesize unbundleSourceClearButton = _unbundleSourceClearButton;
@synthesize unbundleDestinationClearButton = _unbundleDestinationClearButton;
@synthesize unbundleSourceBrowseFilesButton = _unbundleSourceBrowseFilesButton;
@synthesize unbundleSourceBrowseButton = _unbundleSourceBrowseButton;
@synthesize unbundleDestinationBrowseButton = _unbundleDestinationBrowseButton;
@synthesize unbundleRecoverCheckbox = _unbundleRecoverCheckbox;
@synthesize unbundleReadManifestButton = _unbundleReadManifestButton;
@synthesize unbundleActionButton = _unbundleActionButton;
@synthesize progressCancelButton = _progressCancelButton;

+ (CGFloat)requiredHeight {
    return MAX(RequiredBundlePanelHeight(), RequiredUnbundlePanelHeight());
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:0.86 green:0.91 blue:0.90 alpha:1.0].CGColor;
    self.layer.cornerRadius = kUICardCornerRadius;

    [self buildPanels];
    [self setShowsProgressPanel:NO];
    [self applyBundleDefaultsWithSource:nil destination:nil];
    [self applyUnbundleDefaultsWithSource:nil destination:nil];
    [self setActiveHomeTabIndex:0];

    return self;
}

- (void)buildPanels {
    _bundlePanelView = [self buildMakeArchivePanel];
    _unbundlePanelView = [self buildUnbundlePanel];
    _progressPanelView = [[ProgressPanelView alloc] initWithFrame:NSZeroRect];
    _progressCancelButton = _progressPanelView.cancelButton;

    for (NSView *panel in @[_bundlePanelView, _unbundlePanelView, _progressPanelView]) {
        panel.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:panel];
        [NSLayoutConstraint activateConstraints:@[
            [panel.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
            [panel.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
            [panel.topAnchor constraintEqualToAnchor:self.topAnchor],
            [panel.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        ]];
    }
}

- (NSView *)buildMakeArchivePanel {
    NSView *panel = [[NSView alloc] initWithFrame:NSZeroRect];
    panel.translatesAutoresizingMaskIntoConstraints = NO;

    ToolBarTextFieldChunkView *sourceChunk =
        [[ToolBarTextFieldChunkView alloc] initWithText:@""
                                           placeholder:@"input"];
    ToolBarButtonChunkView *sourceBrowseFilesChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Browse Files"];
    ToolBarButtonChunkView *sourceBrowseChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Browse Folder"];
    ToolBarButtonChunkView *sourceClearChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Clear"];
    ToolBarTextFieldChunkView *destinationChunk =
        [[ToolBarTextFieldChunkView alloc] initWithText:@""
                                                placeholder:@"archive"];
    ToolBarButtonChunkView *destinationBrowseChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Browse"];
    ToolBarButtonChunkView *destinationClearChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Clear"];
    ToolBarTextFieldChunkView *prefixChunk =
        [[ToolBarTextFieldChunkView alloc] initWithText:@"archive"
                                           placeholder:@"archive"];
    ToolBarButtonChunkView *prefixClearChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Clear"];
    ToolBarComboBoxChunkView *blockCountChunk =
        [[ToolBarComboBoxChunkView alloc] initWithItems:@[
            @"1 block",
            @"5 blocks",
            @"10 blocks",
            @"25 blocks",
            @"50 blocks",
            @"100 blocks",
            @"250 blocks",
            @"500 blocks",
            @"1000 blocks",
            @"2000 blocks",
        ]];

    _bundleSourceChunkView = sourceChunk;
    _bundleDestinationChunkView = destinationChunk;
    _bundleSourceTextField = sourceChunk.textField;
    _bundleDestinationTextField = destinationChunk.textField;
    _bundleFilePrefixTextField = prefixChunk.textField;
    _bundleSourceClearButton = sourceClearChunk.button;
    _bundleDestinationClearButton = destinationClearChunk.button;
    _bundleFilePrefixClearButton = prefixClearChunk.button;
    _bundleSourceBrowseFilesButton = sourceBrowseFilesChunk.button;
    _bundleSourceBrowseButton = sourceBrowseChunk.button;
    _bundleDestinationBrowseButton = destinationBrowseChunk.button;
    _bundleBlockCountCombo = blockCountChunk.comboBox;

    NSStackView *sourceButtonsStack = MakeEqualWidthStackView(@[
        sourceBrowseFilesChunk,
        sourceBrowseChunk,
    ]);
    sourceButtonsStack.wantsLayer = YES;
    sourceButtonsStack.layer.backgroundColor =
        [NSColor colorWithRed:1.0 green:0.0 blue:0.0 alpha:0.18].CGColor;
    NSView *sourceButtonsRegion =
        MakeRightSectionContainer(sourceButtonsStack);
    NSView *sourceRow = MakeTextInputRow(sourceClearChunk,
                                         sourceChunk,
                                         sourceButtonsRegion,
                                         kUIMenuSectionRightWidth);
    NSView *destinationRow = MakeTextInputRow(destinationClearChunk,
                                              destinationChunk,
                                              MakeRightSectionContainer(destinationBrowseChunk),
                                              kUIMenuSectionRightWidth);
    NSView *prefixRow = MakePrefixAndBlockCountRow(prefixChunk,
                                                   prefixClearChunk,
                                                   blockCountChunk);

    ToolPanelView *sourceDestinationPanel =
        [[ToolPanelView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.92 green:0.96 blue:0.95 alpha:1.0]];
    [sourceDestinationPanel addRowViews:@[sourceRow, destinationRow]];

    ToolPanelView *filePrefixPanel =
        [[ToolPanelView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.94 green:0.97 blue:0.93 alpha:1.0]];
    [filePrefixPanel addRowViews:@[prefixRow]];

    NSView *toolbar = [self buildBottomToolbar];
    [panel addSubview:sourceDestinationPanel];
    [panel addSubview:filePrefixPanel];
    [panel addSubview:toolbar];

    [NSLayoutConstraint activateConstraints:@[
        [sourceDestinationPanel.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor constant:kUIActiveZonePaddingLeft],
        [sourceDestinationPanel.trailingAnchor constraintEqualToAnchor:panel.trailingAnchor constant:-kUIActiveZonePaddingRight],
        [sourceDestinationPanel.topAnchor constraintEqualToAnchor:panel.topAnchor constant:kUIActiveZonePaddingTop],
        [sourceDestinationPanel.heightAnchor constraintEqualToConstant:[ToolPanelView requiredHeightForRowCount:2]],

        [filePrefixPanel.leadingAnchor constraintEqualToAnchor:sourceDestinationPanel.leadingAnchor],
        [filePrefixPanel.trailingAnchor constraintEqualToAnchor:sourceDestinationPanel.trailingAnchor],
        [filePrefixPanel.topAnchor constraintEqualToAnchor:sourceDestinationPanel.bottomAnchor constant:kUIPanelSpacingV],
        [filePrefixPanel.heightAnchor constraintEqualToConstant:[ToolPanelView requiredHeightForRowCount:1]],

        [toolbar.leadingAnchor constraintEqualToAnchor:sourceDestinationPanel.leadingAnchor],
        [toolbar.trailingAnchor constraintEqualToAnchor:sourceDestinationPanel.trailingAnchor],
        [toolbar.topAnchor constraintEqualToAnchor:filePrefixPanel.bottomAnchor constant:kUIPanelSpacingV],
        [toolbar.bottomAnchor constraintLessThanOrEqualToAnchor:panel.bottomAnchor],
    ]];

    return panel;
}

- (NSView *)buildUnbundlePanel {
    NSView *panel = [[NSView alloc] initWithFrame:NSZeroRect];
    panel.translatesAutoresizingMaskIntoConstraints = NO;

    ToolBarTextFieldChunkView *sourceChunk =
        [[ToolBarTextFieldChunkView alloc] initWithText:@""
                                           placeholder:@"input"];
    ToolBarButtonChunkView *sourceBrowseFilesChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Browse Files"];
    ToolBarButtonChunkView *sourceBrowseChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Browse Folder"];
    ToolBarButtonChunkView *sourceClearChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Clear"];
    ToolBarTextFieldChunkView *destinationChunk =
        [[ToolBarTextFieldChunkView alloc] initWithText:@""
                                           placeholder:@"unbundled"];
    ToolBarButtonChunkView *destinationBrowseChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Browse"];
    ToolBarButtonChunkView *destinationClearChunk =
        [[ToolBarButtonChunkView alloc] initWithTitle:@"Clear"];
    ToolBarCheckBoxRightAlignedChunkView *recoverChunk =
        [[ToolBarCheckBoxRightAlignedChunkView alloc] initWithTitle:@"Recover" state:NO];
    ToolBarTextFieldChunkView *passwordChunk =
        [[ToolBarTextFieldChunkView alloc] initWithText:@""
                                           placeholder:@"Password"];
    MainActionButton *readManifestActionView =
        [[MainActionButton alloc] initWithTitle:@"Read Manifest"];
    MainActionButton *unbundleActionView =
        [[MainActionButton alloc] initWithTitle:@"Unbundle"];

    _unbundleSourceChunkView = sourceChunk;
    _unbundleDestinationChunkView = destinationChunk;
    _unbundleSourceBrowseFilesChunkView = sourceBrowseFilesChunk;
    _unbundleSourceBrowseFolderChunkView = sourceBrowseChunk;
    _unbundleRecoverChunkView = recoverChunk;
    _unbundleSourceTextField = sourceChunk.textField;
    _unbundleDestinationTextField = destinationChunk.textField;
    _unbundlePasswordTextField = passwordChunk.textField;
    _unbundleSourceClearButton = sourceClearChunk.button;
    _unbundleDestinationClearButton = destinationClearChunk.button;
    _unbundleSourceBrowseFilesButton = sourceBrowseFilesChunk.button;
    _unbundleSourceBrowseButton = sourceBrowseChunk.button;
    _unbundleDestinationBrowseButton = destinationBrowseChunk.button;
    _unbundleRecoverCheckbox = recoverChunk.checkBox;
    _unbundleReadManifestButton = readManifestActionView.button;
    _unbundleActionView = unbundleActionView;
    _unbundleActionButton = unbundleActionView.button;

    NSStackView *sourceButtonsStack = MakeEqualWidthStackView(@[
        sourceBrowseFilesChunk,
        sourceBrowseChunk,
    ]);
    sourceButtonsStack.wantsLayer = YES;
    sourceButtonsStack.layer.backgroundColor =
        [NSColor colorWithRed:1.0 green:0.0 blue:0.0 alpha:0.18].CGColor;

    NSView *sourceRow = MakeTextInputRow(sourceClearChunk,
                                         sourceChunk,
                                         MakeRightSectionContainer(sourceButtonsStack),
                                         kUIMenuSectionRightWidth);
    NSView *destinationRow = MakeTextInputRow(destinationClearChunk,
                                              destinationChunk,
                                              MakeRightSectionContainer(destinationBrowseChunk),
                                              kUIMenuSectionRightWidth);

    ToolPanelView *sourceDestinationPanel =
        [[ToolPanelView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.92 green:0.96 blue:0.95 alpha:1.0]];
    [sourceDestinationPanel addRowViews:@[sourceRow, destinationRow]];

    NSView *toolbar = [[NSView alloc] initWithFrame:NSZeroRect];
    toolbar.translatesAutoresizingMaskIntoConstraints = NO;

    ToolPanelView *readManifestPanel =
        [[ToolPanelView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.96 green:0.92 blue:0.82 alpha:1.0]];
    [readManifestPanel addSubview:readManifestActionView];
    [readManifestPanel.widthAnchor constraintEqualToConstant:(kUIMenuSectionRightWidth + kUIPanelPaddingLeft + kUIPanelPaddingRight)].active = YES;
    [readManifestPanel.heightAnchor constraintEqualToConstant:[ToolPanelView requiredHeightForRowCount:2]].active = YES;
    [NSLayoutConstraint activateConstraints:@[
        [readManifestActionView.leadingAnchor constraintEqualToAnchor:readManifestPanel.leadingAnchor constant:kUIPanelPaddingLeft],
        [readManifestActionView.trailingAnchor constraintEqualToAnchor:readManifestPanel.trailingAnchor constant:-kUIPanelPaddingRight],
        [readManifestActionView.topAnchor constraintEqualToAnchor:readManifestPanel.topAnchor constant:kUIPanelPaddingTop],
        [readManifestActionView.bottomAnchor constraintEqualToAnchor:readManifestPanel.bottomAnchor constant:-kUIPanelPaddingBottom],
    ]];

    ToolPanelView *passwordPanel =
        [[ToolPanelView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.90 green:0.92 blue:0.98 alpha:1.0]];
    NSView *recoverRow = MakeTrailingAlignedRightSection(recoverChunk, kUICheckBoxChunkWidth);
    [passwordPanel addRowViews:@[recoverRow, MakeSingleFillRow(passwordChunk)]];

    ToolPanelView *actionPanel =
        [[ToolPanelView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.90 green:0.95 blue:0.87 alpha:1.0]];
    [actionPanel addSubview:unbundleActionView];
    [actionPanel.widthAnchor constraintEqualToConstant:(kUIMenuSectionRightWidth + kUIPanelPaddingLeft + kUIPanelPaddingRight)].active = YES;
    [actionPanel.heightAnchor constraintEqualToConstant:[ToolPanelView requiredHeightForRowCount:2]].active = YES;
    [NSLayoutConstraint activateConstraints:@[
        [unbundleActionView.leadingAnchor constraintEqualToAnchor:actionPanel.leadingAnchor constant:kUIPanelPaddingLeft],
        [unbundleActionView.trailingAnchor constraintEqualToAnchor:actionPanel.trailingAnchor constant:-kUIPanelPaddingRight],
        [unbundleActionView.topAnchor constraintEqualToAnchor:actionPanel.topAnchor constant:kUIPanelPaddingTop],
        [unbundleActionView.bottomAnchor constraintEqualToAnchor:actionPanel.bottomAnchor constant:-kUIPanelPaddingBottom],
    ]];

    NSStackView *panelsRow = [[NSStackView alloc] initWithFrame:NSZeroRect];
    panelsRow.translatesAutoresizingMaskIntoConstraints = NO;
    panelsRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    panelsRow.alignment = NSLayoutAttributeTop;
    panelsRow.distribution = NSStackViewDistributionFill;
    panelsRow.spacing = kUIPanelSpacingH;
    [panelsRow addArrangedSubview:readManifestPanel];
    [panelsRow addArrangedSubview:passwordPanel];
    [panelsRow addArrangedSubview:actionPanel];

    [toolbar addSubview:panelsRow];
    [NSLayoutConstraint activateConstraints:@[
        [panelsRow.leadingAnchor constraintEqualToAnchor:toolbar.leadingAnchor],
        [panelsRow.trailingAnchor constraintEqualToAnchor:toolbar.trailingAnchor],
        [panelsRow.topAnchor constraintEqualToAnchor:toolbar.topAnchor],
        [panelsRow.bottomAnchor constraintEqualToAnchor:toolbar.bottomAnchor],
    ]];

    [panel addSubview:sourceDestinationPanel];
    [panel addSubview:toolbar];
    [NSLayoutConstraint activateConstraints:@[
        [sourceDestinationPanel.leadingAnchor constraintEqualToAnchor:panel.leadingAnchor constant:kUIActiveZonePaddingLeft],
        [sourceDestinationPanel.trailingAnchor constraintEqualToAnchor:panel.trailingAnchor constant:-kUIActiveZonePaddingRight],
        [sourceDestinationPanel.topAnchor constraintEqualToAnchor:panel.topAnchor constant:kUIActiveZonePaddingTop],
        [sourceDestinationPanel.heightAnchor constraintEqualToConstant:[ToolPanelView requiredHeightForRowCount:2]],

        [toolbar.leadingAnchor constraintEqualToAnchor:sourceDestinationPanel.leadingAnchor],
        [toolbar.trailingAnchor constraintEqualToAnchor:sourceDestinationPanel.trailingAnchor],
        [toolbar.topAnchor constraintEqualToAnchor:sourceDestinationPanel.bottomAnchor constant:kUIPanelSpacingV],
        [toolbar.bottomAnchor constraintLessThanOrEqualToAnchor:panel.bottomAnchor],
    ]];

    return panel;
}

- (NSView *)buildBottomToolbar {
    NSView *toolbar = [[NSView alloc] initWithFrame:NSZeroRect];
    toolbar.translatesAutoresizingMaskIntoConstraints = NO;

    ToolBarCheckBoxChunkView *repairChunk =
        [[ToolBarCheckBoxChunkView alloc] initWithTitle:@"Repair Sector" state:YES];
    ToolBarCheckBoxChunkView *includePreviewChunk =
        [[ToolBarCheckBoxChunkView alloc] initWithTitle:@"Include Preview" state:YES];
    ToolBarComboBoxChunkView *repairSizeChunk =
        [[ToolBarComboBoxChunkView alloc] initWithItems:@[@"20%", @"40%", @"60%", @"80%"]];

    _bundleRepairCheckbox = repairChunk.checkBox;
    _bundleIncludePreviewCheckbox = includePreviewChunk.checkBox;
    _bundleRepairSizeCombo = repairSizeChunk.comboBox;

    NSStackView *resilienceRowOne = MakeEqualWidthStackView(@[repairChunk, includePreviewChunk]);
    NSView *resilienceRowTwo = MakeSingleFillRow(repairSizeChunk);
    ToolPanelView *resiliencePanel =
        [[ToolPanelView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.96 green:0.92 blue:0.82 alpha:1.0]];
    [resiliencePanel addRowViews:@[resilienceRowOne, resilienceRowTwo]];
    [resiliencePanel.widthAnchor constraintEqualToConstant:kUIResilienceBoxWidth].active = YES;

    ToolBarCheckBoxChunkView *encryptChunk =
        [[ToolBarCheckBoxChunkView alloc] initWithTitle:@"Encrypt" state:YES];
    ToolBarComboBoxChunkView *encryptionChunk =
        [[ToolBarComboBoxChunkView alloc] initWithItems:@[@"Encryption: Low", @"Encryption: Medium", @"Encryption: High"]];
    ToolBarComboBoxChunkView *tableChunk =
        [[ToolBarComboBoxChunkView alloc] initWithItems:@[@"Tables: Low", @"Tables: Medium", @"Tables: High"]];
    ToolBarTextFieldChunkView *passwordChunk =
        [[ToolBarTextFieldChunkView alloc] initWithText:@""
                                           placeholder:@"Password"];

    _bundleEncryptCheckbox = encryptChunk.checkBox;
    _bundleEncryptionStrengthCombo = encryptionChunk.comboBox;
    _bundleTableStrengthCombo = tableChunk.comboBox;
    _bundlePasswordTextField = passwordChunk.textField;
    NSView *encryptionRowOne = MakeEncryptionControlsRow(encryptChunk, encryptionChunk, tableChunk);
    NSView *encryptionRowTwo = MakeSingleFillRow(passwordChunk);
    ToolPanelView *encryptionPanel =
        [[ToolPanelView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.90 green:0.92 blue:0.98 alpha:1.0]];
    [encryptionPanel addRowViews:@[encryptionRowOne, encryptionRowTwo]];

    MainActionButton *bundleActionView =
        [[MainActionButton alloc] initWithTitle:@"Bundle"];
    ToolPanelView *bundleActionPanel =
        [[ToolPanelView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.90 green:0.95 blue:0.87 alpha:1.0]];
    [bundleActionPanel addSubview:bundleActionView];
    [bundleActionPanel.widthAnchor constraintEqualToConstant:(kUIMenuSectionRightWidth + kUIPanelPaddingLeft + kUIPanelPaddingRight)].active = YES;
    [bundleActionPanel.heightAnchor constraintEqualToConstant:[ToolPanelView requiredHeightForRowCount:2]].active = YES;
    [NSLayoutConstraint activateConstraints:@[
        [bundleActionView.leadingAnchor constraintEqualToAnchor:bundleActionPanel.leadingAnchor constant:kUIPanelPaddingLeft],
        [bundleActionView.trailingAnchor constraintEqualToAnchor:bundleActionPanel.trailingAnchor constant:-kUIPanelPaddingRight],
        [bundleActionView.topAnchor constraintEqualToAnchor:bundleActionPanel.topAnchor constant:kUIPanelPaddingTop],
        [bundleActionView.bottomAnchor constraintEqualToAnchor:bundleActionPanel.bottomAnchor constant:-kUIPanelPaddingBottom],
    ]];

    _bundleActionButton = bundleActionView.button;

    NSStackView *panelsRow = [[NSStackView alloc] initWithFrame:NSZeroRect];
    panelsRow.translatesAutoresizingMaskIntoConstraints = NO;
    panelsRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    panelsRow.alignment = NSLayoutAttributeTop;
    panelsRow.distribution = NSStackViewDistributionFill;
    panelsRow.spacing = kUIPanelSpacingH;
    [panelsRow addArrangedSubview:resiliencePanel];
    [panelsRow addArrangedSubview:encryptionPanel];
    [panelsRow addArrangedSubview:bundleActionPanel];

    [toolbar addSubview:panelsRow];
    [NSLayoutConstraint activateConstraints:@[
        [panelsRow.leadingAnchor constraintEqualToAnchor:toolbar.leadingAnchor],
        [panelsRow.trailingAnchor constraintEqualToAnchor:toolbar.trailingAnchor],
        [panelsRow.topAnchor constraintEqualToAnchor:toolbar.topAnchor],
        [panelsRow.bottomAnchor constraintEqualToAnchor:toolbar.bottomAnchor],
    ]];

    return toolbar;
}

- (void)applyBundleDefaultsWithSource:(NSString *)source
                          destination:(NSString *)destination {
    self.bundleSourceTextField.stringValue =
        (source.length > 0) ? source : @"input";
    self.bundleDestinationTextField.stringValue =
        (destination.length > 0) ? destination : @"archive";
}

- (void)applyUnbundleDefaultsWithSource:(NSString *)source
                            destination:(NSString *)destination {
    self.unbundleSourceTextField.stringValue =
        (source.length > 0) ? source : @"archive";
    self.unbundleDestinationTextField.stringValue =
        (destination.length > 0) ? destination : @"unbundled";
}

- (void)setBundleSourcePathDropHandler:(void (^)(NSString *path))handler {
    _bundleSourceChunkView.pathDropHandler = handler;
}

- (void)setBundleDestinationPathDropHandler:(void (^)(NSString *path))handler {
    _bundleDestinationChunkView.pathDropHandler = handler;
}

- (void)setUnbundleSourcePathDropHandler:(void (^)(NSString *path))handler {
    _unbundleSourceChunkView.pathDropHandler = handler;
}

- (void)setUnbundleDestinationPathDropHandler:(void (^)(NSString *path))handler {
    _unbundleDestinationChunkView.pathDropHandler = handler;
}

- (void)setActiveHomeTabIndex:(NSInteger)activeHomeTabIndex {
    if (_activeHomeTabIndex == 1) {
        _unbundleRecoverEnabled =
            (self.unbundleRecoverCheckbox.state == NSControlStateValueOn);
    } else if (_activeHomeTabIndex == 2) {
        _repairAggressiveEnabled =
            (self.unbundleRecoverCheckbox.state == NSControlStateValueOn);
    } else if (_activeHomeTabIndex == 3) {
        _folderCompareIgnoreHiddenEnabled =
            (self.unbundleRecoverCheckbox.state == NSControlStateValueOn);
    }

    _activeHomeTabIndex = activeHomeTabIndex;
    BOOL showsBundlePanel = (activeHomeTabIndex == 0);
    BOOL showsUnbundlePanel = (activeHomeTabIndex == 1 || activeHomeTabIndex == 2 || activeHomeTabIndex == 3);
    BOOL showsOptionControls = (activeHomeTabIndex == 1 || activeHomeTabIndex == 2 || activeHomeTabIndex == 3);
    BOOL showsSourceBrowseFiles = (activeHomeTabIndex == 1);

    _bundlePanelView.hidden = !showsBundlePanel || self.showsProgressPanel;
    _unbundlePanelView.hidden = !showsUnbundlePanel || self.showsProgressPanel;
    _unbundleSourceBrowseFilesChunkView.hidden = !showsSourceBrowseFiles;
    _unbundleRecoverChunkView.hidden = !showsOptionControls;
    if (activeHomeTabIndex == 3) {
        [self.unbundleRecoverCheckbox setTitle:@"Ignore hidden"];
        self.unbundleRecoverCheckbox.state =
            _folderCompareIgnoreHiddenEnabled ? NSControlStateValueOn : NSControlStateValueOff;
    } else if (activeHomeTabIndex == 2) {
        [self.unbundleRecoverCheckbox setTitle:@"Aggressive"];
        self.unbundleRecoverCheckbox.state =
            _repairAggressiveEnabled ? NSControlStateValueOn : NSControlStateValueOff;
    } else {
        [self.unbundleRecoverCheckbox setTitle:@"Recover"];
        self.unbundleRecoverCheckbox.state =
            _unbundleRecoverEnabled ? NSControlStateValueOn : NSControlStateValueOff;
    }
    if (activeHomeTabIndex == 2) {
        [_unbundleActionButton setTitle:@"Repair"];
    } else if (activeHomeTabIndex == 3) {
        [_unbundleActionButton setTitle:@"Folder Compare"];
    } else {
        [_unbundleActionButton setTitle:@"Unbundle"];
    }
}

- (void)setShowsProgressPanel:(BOOL)showsProgressPanel {
    _showsProgressPanel = showsProgressPanel;
    _bundlePanelView.hidden = showsProgressPanel || (self.activeHomeTabIndex != 0);
    _unbundlePanelView.hidden = showsProgressPanel || !(self.activeHomeTabIndex == 1 || self.activeHomeTabIndex == 2 || self.activeHomeTabIndex == 3);
    _progressPanelView.hidden = !showsProgressPanel;
}

- (void)setBundleControlsEnabled:(BOOL)enabled {
    for (NSControl *control in @[
            self.bundleSourceTextField,
            self.bundleDestinationTextField,
            self.bundleFilePrefixTextField,
            self.bundleSourceClearButton,
            self.bundleDestinationClearButton,
            self.bundleFilePrefixClearButton,
            self.bundleSourceBrowseFilesButton,
            self.bundleSourceBrowseButton,
            self.bundleDestinationBrowseButton,
            self.bundleRepairCheckbox,
            self.bundleIncludePreviewCheckbox,
            self.bundleRepairSizeCombo,
            self.bundleEncryptCheckbox,
            self.bundleEncryptionStrengthCombo,
            self.bundleTableStrengthCombo,
            self.bundlePasswordTextField,
            self.bundleBlockCountCombo,
            self.bundleActionButton,
            self.unbundleSourceTextField,
            self.unbundleDestinationTextField,
            self.unbundlePasswordTextField,
            self.unbundleSourceClearButton,
            self.unbundleDestinationClearButton,
            self.unbundleSourceBrowseFilesButton,
            self.unbundleSourceBrowseButton,
            self.unbundleDestinationBrowseButton,
            self.unbundleRecoverCheckbox,
            self.unbundleReadManifestButton,
            self.unbundleActionButton
         ]) {
        control.enabled = enabled;
    }
}

- (void)updateProgressTitle:(NSString *)title
                     detail:(NSString *)detail
                   fraction:(double)fraction {
    (void)detail;
    _progressPanelView.titleLabel.stringValue = title ?: @"Working";
    _progressPanelView.progressIndicator.doubleValue =
        MAX(0.0, MIN(100.0, fraction * 100.0));
}

@end

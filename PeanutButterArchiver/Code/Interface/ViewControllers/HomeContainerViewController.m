#import "HomeContainerViewController.hpp"

#import "../UIConstants.hpp"
#import "../Views/HomeActiveModeContainerView.hpp"
#import "../Views/HomeFooterView.hpp"
#import "../Views/HomeHeaderView.hpp"
#import "../Views/HomeLogContainerView.hpp"
#import "../Views/HomeToolViewSplitter.hpp"
#import "../Views/HomeToolViewTop.hpp"

@implementation HomeContainerViewController

- (BOOL)canStartPrimaryAction {
    return self.uiState == HomeUiStateV2Home;
}

- (void)loadView {
    NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, kUIWindowWidth, kUIWindowHeight)];
    rootView.translatesAutoresizingMaskIntoConstraints = NO;
    rootView.wantsLayer = YES;
    rootView.layer.backgroundColor = [NSColor colorWithRed:0.95 green:0.94 blue:0.90 alpha:1.0].CGColor;
    self.view = rootView;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    self.homeHeaderView = [[HomeHeaderView alloc] initWithFrame:NSZeroRect];
    self.homeToolViewTop = [[HomeToolViewTop alloc] initWithFrame:NSZeroRect];
    self.homeActiveModeContainerView = [[HomeActiveModeContainerView alloc] initWithFrame:NSZeroRect];
    self.homeToolViewSplitter = [[HomeToolViewSplitter alloc] initWithFrame:NSZeroRect];
    self.homeLogContainerView = [[HomeLogContainerView alloc] initWithFrame:NSZeroRect];
    self.homeFooterView = [[HomeFooterView alloc] initWithFrame:NSZeroRect];

    NSArray<NSView *> *views = @[
        self.homeHeaderView,
        self.homeToolViewTop,
        self.homeActiveModeContainerView,
        self.homeToolViewSplitter,
        self.homeLogContainerView,
        self.homeFooterView
    ];

    for (NSView *view in views) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [self.view addSubview:view];
    }

    [NSLayoutConstraint activateConstraints:@[
        [self.homeHeaderView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:kUIGlobalPaddingLeft],
        [self.homeHeaderView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-kUIGlobalPaddingRight],
        [self.homeHeaderView.topAnchor constraintEqualToAnchor:self.view.topAnchor constant:kUIGlobalPaddingTop],
        [self.homeHeaderView.heightAnchor constraintEqualToConstant:kUIHeaderHeight],

        [self.homeToolViewTop.leadingAnchor constraintEqualToAnchor:self.homeHeaderView.leadingAnchor],
        [self.homeToolViewTop.trailingAnchor constraintEqualToAnchor:self.homeHeaderView.trailingAnchor],
        [self.homeToolViewTop.topAnchor constraintEqualToAnchor:self.homeHeaderView.bottomAnchor constant:kUITopToolbarActiveContentSpacing],
        [self.homeToolViewTop.heightAnchor constraintEqualToConstant:kUIToolbarHeight],

        [self.homeActiveModeContainerView.leadingAnchor constraintEqualToAnchor:self.homeHeaderView.leadingAnchor constant:kActiveAreaPaddingLeft],
        [self.homeActiveModeContainerView.trailingAnchor constraintEqualToAnchor:self.homeHeaderView.trailingAnchor constant:-kActiveAreaPaddingRight],
        [self.homeActiveModeContainerView.topAnchor constraintEqualToAnchor:self.homeToolViewTop.bottomAnchor constant:kActiveAreaPaddingTop],
        [self.homeActiveModeContainerView.heightAnchor constraintEqualToConstant:[HomeActiveModeContainerView requiredHeight]],

        [self.homeToolViewSplitter.leadingAnchor constraintEqualToAnchor:self.homeHeaderView.leadingAnchor],
        [self.homeToolViewSplitter.trailingAnchor constraintEqualToAnchor:self.homeHeaderView.trailingAnchor],
        [self.homeToolViewSplitter.topAnchor constraintEqualToAnchor:self.homeActiveModeContainerView.bottomAnchor constant:(kActiveAreaPaddingBottom + kActiveAreaSpacingV)],
        [self.homeToolViewSplitter.heightAnchor constraintEqualToConstant:kUIToolViewSplitterHeight],

        [self.homeLogContainerView.leadingAnchor constraintEqualToAnchor:self.homeHeaderView.leadingAnchor],
        [self.homeLogContainerView.trailingAnchor constraintEqualToAnchor:self.homeHeaderView.trailingAnchor],
        [self.homeLogContainerView.topAnchor constraintEqualToAnchor:self.homeToolViewSplitter.bottomAnchor constant:kUIMiddleToolbarDebugContainerSpacing],
        [self.homeLogContainerView.bottomAnchor constraintEqualToAnchor:self.homeFooterView.topAnchor constant:-kUIDebugContainerFooterSpacing],

        [self.homeFooterView.leadingAnchor constraintEqualToAnchor:self.homeHeaderView.leadingAnchor],
        [self.homeFooterView.trailingAnchor constraintEqualToAnchor:self.homeHeaderView.trailingAnchor],
        [self.homeFooterView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor constant:-kUIFooterPaddingBottom],
        [self.homeFooterView.heightAnchor constraintEqualToConstant:kUIFooterHeight],
    ]];

    [self transitionToHomeState];
}

- (void)transitionToHomeState {
    _uiState = HomeUiStateV2Home;
    [self.homeActiveModeContainerView setShowsProgressPanel:NO];
    self.homeActiveModeContainerView.alphaValue = 1.0;
    [self.homeActiveModeContainerView updateProgressTitle:@"Working"
                                                   detail:@""
                                                 fraction:0.0];
    self.homeActiveModeContainerView.progressCancelButton.hidden = YES;
    self.homeActiveModeContainerView.progressCancelButton.enabled = NO;
    [self.homeHeaderView setButtonsEnabled:YES];
    self.homeToolViewTop.modeToggle.enabled = YES;
    [self.homeActiveModeContainerView setBundleControlsEnabled:YES];
}

- (void)transitionToGhostStateWithTitle:(NSString *)title
                                 detail:(NSString *)detail {
    (void)title;
    (void)detail;
    _uiState = HomeUiStateV2Ghost;
    [self.homeActiveModeContainerView setShowsProgressPanel:NO];
    self.homeActiveModeContainerView.alphaValue = 0.75;
    [self.homeActiveModeContainerView updateProgressTitle:@"Working"
                                                   detail:@""
                                                 fraction:0.0];
    self.homeActiveModeContainerView.progressCancelButton.hidden = YES;
    self.homeActiveModeContainerView.progressCancelButton.enabled = NO;
    [self.homeHeaderView setButtonsEnabled:NO];
    self.homeToolViewTop.modeToggle.enabled = NO;
    [self.homeActiveModeContainerView setBundleControlsEnabled:NO];
}

- (void)transitionToLoadingStateWithTitle:(NSString *)title
                                   detail:(NSString *)detail
                                 fraction:(double)fraction
                            cancelEnabled:(BOOL)cancelEnabled {
    _uiState = HomeUiStateV2Loading;
    [self.homeActiveModeContainerView setShowsProgressPanel:YES];
    self.homeActiveModeContainerView.alphaValue = 1.0;
    self.homeActiveModeContainerView.progressCancelButton.hidden = NO;
    self.homeActiveModeContainerView.progressCancelButton.enabled = cancelEnabled;
    [self.homeActiveModeContainerView
        updateProgressTitle:(title.length > 0 ? title : @"Working")
                      detail:(detail.length > 0 ? detail : @"")
                    fraction:fraction];
    [self.homeHeaderView setButtonsEnabled:NO];
    self.homeToolViewTop.modeToggle.enabled = YES;
    [self.homeActiveModeContainerView setBundleControlsEnabled:NO];
}

@end

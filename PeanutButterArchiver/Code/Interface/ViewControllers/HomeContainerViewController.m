#import "HomeContainerViewController.hpp"

#import "../UIConstants.hpp"
#import "../Views/HomeActiveModeContainerView.hpp"
#import "../Views/HomeHeaderView.hpp"
#import "../Views/HomeLogControlView.hpp"
#import "../Views/HomeLogContainerView.hpp"
#import "../Views/HomeLogSeparatorView.hpp"
#import "../Views/HomeToolViewTop.hpp"

@implementation HomeContainerViewController

- (BOOL)canStartPrimaryAction {
    return self.uiState == HomeUiStateV2Home;
}

- (void)loadView {
    NSView *rootView = [[NSView alloc] initWithFrame:NSMakeRect(0.0, 0.0, kUIWindowWidth, kUIWindowHeight)];
    rootView.translatesAutoresizingMaskIntoConstraints = NO;
    self.view = rootView;
}

- (void)viewDidLoad {
    [super viewDidLoad];

    //self.homeHeaderView = [[HomeHeaderView alloc] initWithFrame:NSZeroRect];
    self.homeToolViewTop = [[HomeToolViewTop alloc] initWithFrame:NSZeroRect];
    self.homeActiveModeContainerView = [[HomeActiveModeContainerView alloc] initWithFrame:NSZeroRect];
    self.homeLogSeparatorView = [[HomeLogSeparatorView alloc] initWithFrame:NSZeroRect];
    self.homeLogContainerView = [[HomeLogContainerView alloc] initWithFrame:NSZeroRect];
    self.homeLogControlView = [[HomeLogControlView alloc] initWithFrame:NSZeroRect];

    NSArray<NSView *> *views = @[
        //self.homeHeaderView,
        self.homeToolViewTop,
        self.homeActiveModeContainerView,
        self.homeLogSeparatorView,
        self.homeLogContainerView,
        self.homeLogControlView
    ];

    for (NSView *view in views) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [self.view addSubview:view];
    }

    [NSLayoutConstraint activateConstraints:@[
        
        [self.homeToolViewTop.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:kUIGlobalPaddingLeft],
        [self.homeToolViewTop.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-kUIGlobalPaddingRight],
        [self.homeToolViewTop.topAnchor constraintEqualToAnchor:self.view.topAnchor constant:kUIGlobalPaddingTop],
        [self.homeToolViewTop.heightAnchor constraintEqualToConstant:kUIToolbarHeight],

        [self.homeActiveModeContainerView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:kUIGlobalPaddingLeft],
        [self.homeActiveModeContainerView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-kUIGlobalPaddingRight],
        [self.homeActiveModeContainerView.topAnchor constraintEqualToAnchor:self.homeToolViewTop.bottomAnchor constant:kActiveAreaPaddingTop],
        [self.homeActiveModeContainerView.heightAnchor constraintEqualToConstant:[HomeActiveModeContainerView requiredHeight]],

        [self.homeLogSeparatorView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:kUIGlobalPaddingLeft],
        [self.homeLogSeparatorView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-kUIGlobalPaddingRight],
        [self.homeLogSeparatorView.topAnchor constraintEqualToAnchor:self.homeActiveModeContainerView.bottomAnchor constant:(kActiveAreaPaddingBottom + kActiveAreaSpacingV)],
        [self.homeLogSeparatorView.heightAnchor constraintEqualToConstant:kUILogSeparatorHeight],

        
        [self.homeLogContainerView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:kUIGlobalPaddingLeft],
        [self.homeLogContainerView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-kUIGlobalPaddingRight],
        [self.homeLogContainerView.topAnchor constraintEqualToAnchor:self.homeLogSeparatorView.bottomAnchor constant:kUIMiddleToolbarDebugContainerSpacing],
        [self.homeLogContainerView.bottomAnchor constraintEqualToAnchor:self.homeLogControlView.topAnchor constant:-kUIDebugContainerFooterSpacing],
        
        [self.homeLogControlView.leadingAnchor constraintEqualToAnchor:self.view.leadingAnchor constant:kUIGlobalPaddingLeft],
        [self.homeLogControlView.trailingAnchor constraintEqualToAnchor:self.view.trailingAnchor constant:-kUIGlobalPaddingRight],
        [self.homeLogControlView.bottomAnchor constraintEqualToAnchor:self.view.bottomAnchor constant:-kUIFooterPaddingBottom],
        [self.homeLogControlView.heightAnchor constraintEqualToConstant:kUIFooterHeight],
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
    self.homeToolViewTop.modeToggle.enabled = YES;
    [self.homeActiveModeContainerView setBundleControlsEnabled:NO];
}

@end

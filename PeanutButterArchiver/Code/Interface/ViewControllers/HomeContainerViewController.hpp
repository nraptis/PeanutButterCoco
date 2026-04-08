#import <Cocoa/Cocoa.h>

//@class HomeHeaderView;
@class HomeToolViewTop;
@class HomeActiveModeContainerView;
@class HomeLogSeparatorView;
@class HomeLogContainerView;
@class HomeLogControlView;

typedef NS_ENUM(NSInteger, HomeUiStateV2) {
    HomeUiStateV2Home = 0,
    HomeUiStateV2Ghost = 1,
    HomeUiStateV2Loading = 2,
};

@interface HomeContainerViewController : NSViewController

//@property (strong, nonatomic) HomeHeaderView *homeHeaderView;
@property (strong, nonatomic) HomeToolViewTop *homeToolViewTop;
@property (strong, nonatomic) HomeActiveModeContainerView *homeActiveModeContainerView;
@property (strong, nonatomic) HomeLogSeparatorView *homeLogSeparatorView;
@property (strong, nonatomic) HomeLogContainerView *homeLogContainerView;
@property (strong, nonatomic) HomeLogControlView *homeLogControlView;
@property (assign, nonatomic, readonly) HomeUiStateV2 uiState;

- (BOOL)canStartPrimaryAction;
- (void)transitionToHomeState;
- (void)transitionToGhostStateWithTitle:(NSString *)title
                                 detail:(NSString *)detail;
- (void)transitionToLoadingStateWithTitle:(NSString *)title
                                   detail:(NSString *)detail
                                 fraction:(double)fraction
                            cancelEnabled:(BOOL)cancelEnabled;

@end

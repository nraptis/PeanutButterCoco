#import <Cocoa/Cocoa.h>

typedef NS_ENUM(NSInteger, HomeHeaderActionV2) {
    HomeHeaderActionV2GreenDelaySucceed = 0,
    HomeHeaderActionV2GreenDelayFail = 1,
    HomeHeaderActionV2YellowDelaySucceed = 2,
    HomeHeaderActionV2YellowDelayFail = 3,
    HomeHeaderActionV2Red = 4,
};

@class HomeHeaderView;

@protocol HomeHeaderViewDelegate <NSObject>

- (void)homeHeaderView:(HomeHeaderView *)homeHeaderView didTriggerAction:(HomeHeaderActionV2)action;

@end

@interface HomeHeaderView : NSView

@property (weak, nonatomic) id<HomeHeaderViewDelegate> delegate;

- (void)setButtonsEnabled:(BOOL)enabled;

@end

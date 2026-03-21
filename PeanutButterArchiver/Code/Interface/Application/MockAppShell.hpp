#import <Cocoa/Cocoa.h>

@class HomeContainerViewController;

@interface MockAppShell : NSObject

- (instancetype)initWithHomeContainerViewController:(HomeContainerViewController *)homeContainerViewController;
- (void)startPolling;
- (void)stopPolling;
- (void)enqueueGreenDelaySucceed;
- (void)enqueueGreenDelayFail;
- (void)enqueueYellowDelaySucceed;
- (void)enqueueYellowDelayFail;
- (void)enqueueRed;
- (void)enqueueCancelRequest;
- (BOOL)hasActivePrimaryAction;

@end

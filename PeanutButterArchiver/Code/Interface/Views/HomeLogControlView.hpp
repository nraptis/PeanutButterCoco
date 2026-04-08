#import <Cocoa/Cocoa.h>
#import "PBDrawableButton.hpp"

@interface HomeLogControlView : NSView

@property (strong, nonatomic, readonly) PBDrawableButton *clearLogsButton;
@property (strong, nonatomic, readonly) PBDrawableButton *scrollToBottomButton;
@property (strong, nonatomic, readonly) PBDrawableButton *verboseEventsButton;

@end

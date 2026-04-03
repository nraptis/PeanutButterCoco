#import <Cocoa/Cocoa.h>

@interface HomeToolViewSplitter : NSView

@property (strong, nonatomic, readonly) NSButton *clearLogsButton;
@property (strong, nonatomic, readonly) NSButton *scrollToBottomButton;
@property (strong, nonatomic, readonly) NSButton *verboseEventsCheckbox;

@end

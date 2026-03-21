#import <Cocoa/Cocoa.h>

@interface HomeLogView : NSView

@property (strong, nonatomic, readonly) NSTextView *textView;

- (void)appendLine:(NSString *)line;
- (void)clearAll;
- (void)scrollToBottom;

@end

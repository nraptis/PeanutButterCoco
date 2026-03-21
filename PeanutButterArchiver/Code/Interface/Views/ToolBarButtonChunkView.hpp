#import <Cocoa/Cocoa.h>

@interface ToolBarButtonChunkView : NSView

@property (strong, nonatomic, readonly) NSButton *button;

- (instancetype)initWithTitle:(NSString *)title;

@end

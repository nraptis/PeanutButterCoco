#import <Cocoa/Cocoa.h>

@interface MainActionButton : NSView

@property (strong, nonatomic, readonly) NSButton *button;

- (instancetype)initWithTitle:(NSString *)title;

@end

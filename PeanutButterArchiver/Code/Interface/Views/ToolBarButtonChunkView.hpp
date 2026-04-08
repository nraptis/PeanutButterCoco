#import <Cocoa/Cocoa.h>

@interface ToolBarButtonChunkView : NSView

@property (strong, nonatomic, readonly) NSButton *button;

@property (strong, nonatomic) NSLayoutConstraint *constraintLeft;
@property (strong, nonatomic) NSLayoutConstraint *constraintRight;

- (instancetype)initWithTitle:(NSString *)title;

- (void) setSmallPaddingLeft;
- (void) setSmallPaddingRight;

- (void) setMediumPaddingRight;

@end

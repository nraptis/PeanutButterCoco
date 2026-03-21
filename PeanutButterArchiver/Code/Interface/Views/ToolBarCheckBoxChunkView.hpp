#import <Cocoa/Cocoa.h>

@interface ToolBarCheckBoxChunkView : NSView

@property (strong, nonatomic, readonly) NSButton *checkBox;

- (instancetype)initWithTitle:(NSString *)title
                        state:(BOOL)state;

@end

@interface ToolBarCheckBoxRightAlignedChunkView : NSView

@property (strong, nonatomic, readonly) NSButton *checkBox;

- (instancetype)initWithTitle:(NSString *)title
                        state:(BOOL)state;

@end

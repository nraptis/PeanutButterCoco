#import <Cocoa/Cocoa.h>

@interface ToolBarTextFieldChunkView : NSView

@property (strong, nonatomic, readonly) NSTextField *textField;
@property (copy, nonatomic) void (^pathDropHandler)(NSString *path);

- (instancetype)initWithText:(NSString *)text
                 placeholder:(NSString *)placeholder;

@end

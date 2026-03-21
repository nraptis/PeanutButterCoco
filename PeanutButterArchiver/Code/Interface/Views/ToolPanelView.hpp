#import <Cocoa/Cocoa.h>

@interface ToolPanelView : NSView

+ (CGFloat)requiredHeightForRowCount:(NSUInteger)rowCount;
- (instancetype)initWithBackgroundColor:(NSColor *)backgroundColor;
- (void)addRowViews:(NSArray<NSView *> *)rowViews;

@end

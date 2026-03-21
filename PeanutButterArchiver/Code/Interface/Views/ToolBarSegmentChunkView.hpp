#import <Cocoa/Cocoa.h>

@interface ToolBarSegmentChunkView : NSView

@property (strong, nonatomic, readonly) NSSegmentedControl *segmentedControl;

- (instancetype)initWithSegments:(NSArray<NSString *> *)segments;

@end

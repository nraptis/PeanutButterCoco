#import <Cocoa/Cocoa.h>

@class PBDrawableButton;

@interface MainTabView : NSControl

@property (assign, nonatomic) NSInteger selectedSegment;
@property (assign, nonatomic, readonly) NSInteger segmentCount;
@property (assign, nonatomic) CGFloat segmentHeight;
@property (strong, nonatomic) NSColor *backgroundColorUnselected;
@property (strong, nonatomic) NSColor *backgroundColorUnselectedDepressed;
@property (strong, nonatomic) NSColor *backgroundColorSelected;
@property (strong, nonatomic) NSColor *backgroundColorSelectedDepressed;

- (instancetype)initWithSegments:(NSArray<NSString *> *)segments;
- (NSString *)labelForSegment:(NSInteger)segment;

@end

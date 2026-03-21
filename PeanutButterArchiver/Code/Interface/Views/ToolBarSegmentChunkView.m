#import "ToolBarSegmentChunkView.hpp"

#import "../UIConstants.hpp"

@implementation ToolBarSegmentChunkView

- (instancetype)initWithSegments:(NSArray<NSString *> *)segments {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithWhite:1.0 alpha:0.18].CGColor;
    self.layer.cornerRadius = 8.0;

    _segmentedControl = [[NSSegmentedControl alloc] initWithFrame:NSZeroRect];
    _segmentedControl.translatesAutoresizingMaskIntoConstraints = NO;
    _segmentedControl.segmentCount = segments.count;
    _segmentedControl.trackingMode = NSSegmentSwitchTrackingSelectOne;
    if ([_segmentedControl respondsToSelector:@selector(setSegmentStyle:)]) {
        _segmentedControl.segmentStyle = NSSegmentStyleRounded;
    }
    for (NSInteger i = 0; i < (NSInteger)segments.count; ++i) {
        [_segmentedControl setLabel:(segments[(NSUInteger)i] ?: @"") forSegment:i];
    }

    [self addSubview:_segmentedControl];

    [NSLayoutConstraint activateConstraints:@[
        [_segmentedControl.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:8.0],
        [_segmentedControl.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-8.0],
        [_segmentedControl.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [_segmentedControl.heightAnchor constraintEqualToConstant:kUITopSegmentControlHeight],
        [self.heightAnchor constraintEqualToConstant:kUIToolbarHeight],
    ]];

    return self;
}

@end

#import "HomeToolViewTop.hpp"
#import "ToolBarSegmentChunkView.hpp"
#import "ToolRowSectionView.hpp"

#import "../UIConstants.hpp"

@implementation HomeToolViewTop

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    ToolRowSectionView *sectionView =
        [[ToolRowSectionView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.98 green:0.97 blue:0.94 alpha:1.0]];
    ToolBarSegmentChunkView *segmentView =
        [[ToolBarSegmentChunkView alloc] initWithSegments:@[@"Bundle", @"Unbundle", @"Tools"]];
    self.modeToggle = segmentView.segmentedControl;
    self.modeToggle.selectedSegment = 0;

    [sectionView addSubview:segmentView];
    [self addSubview:sectionView];

    [NSLayoutConstraint activateConstraints:@[
        [sectionView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [sectionView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [sectionView.topAnchor constraintEqualToAnchor:self.topAnchor],
        [sectionView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        [segmentView.leadingAnchor constraintEqualToAnchor:sectionView.leadingAnchor],
        [segmentView.trailingAnchor constraintEqualToAnchor:sectionView.trailingAnchor],
        [segmentView.topAnchor constraintEqualToAnchor:sectionView.topAnchor],
        [segmentView.bottomAnchor constraintEqualToAnchor:sectionView.bottomAnchor],
    ]];

    return self;
}

@end

#import "HomeToolViewTop.hpp"
#import "ToolRowSectionView.hpp"
#import "ToolBarSegmentChunkView.hpp"

#import "../UIConstants.hpp"

@implementation HomeToolViewTop

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    ToolRowSectionView *sectionView =
        [[ToolRowSectionView alloc] initWithBackgroundColor:[NSColor colorWithRed:0.98 green:0.97 blue:0.94 alpha:1.0]];
    ToolBarSegmentChunkView *segmentChunk =
        [[ToolBarSegmentChunkView alloc] initWithSegments:@[@"Bundle", @"Unbundle", @"Repair", @"Folder Compare"]];
    self.modeToggle = segmentChunk.segmentedControl;
    self.modeToggle.selectedSegment = 0;

    [sectionView addSubview:segmentChunk];
    [self addSubview:sectionView];

    [NSLayoutConstraint activateConstraints:@[
        [sectionView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [sectionView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [sectionView.topAnchor constraintEqualToAnchor:self.topAnchor],
        [sectionView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        [segmentChunk.leadingAnchor constraintEqualToAnchor:sectionView.leadingAnchor constant:kUIActiveZonePaddingLeft],
        [segmentChunk.trailingAnchor constraintEqualToAnchor:sectionView.trailingAnchor constant:-kUIActiveZonePaddingRight],
        [segmentChunk.centerYAnchor constraintEqualToAnchor:sectionView.centerYAnchor],
    ]];

    return self;
}

@end

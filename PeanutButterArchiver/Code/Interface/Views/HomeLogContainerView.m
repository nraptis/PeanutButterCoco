#import "HomeLogContainerView.hpp"

#import "../UIConstants.hpp"
#import "HomeLogView.hpp"

@implementation HomeLogContainerView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:0.53 green:0.36 blue:0.20 alpha:1.0].CGColor;
    self.layer.cornerRadius = kUICardCornerRadius;

    _homeLogView = [[HomeLogView alloc] initWithFrame:NSZeroRect];
    [self addSubview:_homeLogView];

    [NSLayoutConstraint activateConstraints:@[
        [_homeLogView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kUILogViewPaddingLeft],
        [_homeLogView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-kUILogViewPaddingRight],
        [_homeLogView.topAnchor constraintEqualToAnchor:self.topAnchor constant:kUILogViewPaddingTop],
        [_homeLogView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-kUILogViewPaddingBottom],
    ]];

    return self;
}

@end

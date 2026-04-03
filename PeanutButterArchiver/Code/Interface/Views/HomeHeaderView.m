#import "HomeHeaderView.hpp"

@implementation HomeHeaderView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:0.90 green:0.84 blue:0.72 alpha:1.0].CGColor;
    self.layer.cornerRadius = 12.0;

    NSTextField *titleLabel = [NSTextField labelWithString:@"PeanutButter Archiver"];
    titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    titleLabel.font = [NSFont systemFontOfSize:15.0 weight:NSFontWeightSemibold];
    titleLabel.textColor = [NSColor colorWithRed:0.24 green:0.20 blue:0.15 alpha:1.0];
    [self addSubview:titleLabel];

    [NSLayoutConstraint activateConstraints:@[
        [titleLabel.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:14.0],
        [titleLabel.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
    ]];

    return self;
}

- (void)setButtonsEnabled:(BOOL)enabled {
    (void)enabled;
}

@end

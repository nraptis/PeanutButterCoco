#import "HomeHeaderView.hpp"

static NSButton *MakeHeaderButton(NSString *title, NSColor *color, NSInteger tag) {
    NSButton *button = [NSButton buttonWithTitle:title target:nil action:nil];
    button.translatesAutoresizingMaskIntoConstraints = NO;
    button.font = [NSFont systemFontOfSize:11.0 weight:NSFontWeightBold];
    button.tag = tag;
    button.bezelStyle = NSBezelStyleRounded;
    button.bezelColor = color;
    return button;
}

@implementation HomeHeaderView {
    NSArray<NSButton *> *_mockButtons;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:0.90 green:0.84 blue:0.72 alpha:1.0].CGColor;
    self.layer.cornerRadius = 12.0;

    NSButton *greenDelaySucceed = MakeHeaderButton(@"Test-Green-Delay-Succeed", [NSColor colorWithRed:0.47 green:0.73 blue:0.40 alpha:1.0], HomeHeaderActionV2GreenDelaySucceed);
    NSButton *greenDelayFail = MakeHeaderButton(@"Test-Green-Delay-Fail", [NSColor colorWithRed:0.39 green:0.66 blue:0.33 alpha:1.0], HomeHeaderActionV2GreenDelayFail);
    NSButton *yellowDelaySucceed = MakeHeaderButton(@"Test-Yellow-Delay-Succeed", [NSColor colorWithRed:0.88 green:0.76 blue:0.30 alpha:1.0], HomeHeaderActionV2YellowDelaySucceed);
    NSButton *yellowDelayFail = MakeHeaderButton(@"Test-Yellow-Delay-Fail", [NSColor colorWithRed:0.82 green:0.63 blue:0.21 alpha:1.0], HomeHeaderActionV2YellowDelayFail);
    NSButton *redButton = MakeHeaderButton(@"Test-Red", [NSColor colorWithRed:0.80 green:0.35 blue:0.31 alpha:1.0], HomeHeaderActionV2Red);

    _mockButtons = @[greenDelaySucceed, greenDelayFail, yellowDelaySucceed, yellowDelayFail, redButton];
    for (NSButton *button in _mockButtons) {
        button.target = self;
        button.action = @selector(handleMockButtonPress:);
    }

    NSStackView *buttonRow = [[NSStackView alloc] initWithFrame:NSZeroRect];
    buttonRow.translatesAutoresizingMaskIntoConstraints = NO;
    buttonRow.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    buttonRow.spacing = 8.0;
    buttonRow.alignment = NSLayoutAttributeCenterY;
    buttonRow.distribution = NSStackViewDistributionFillEqually;
    for (NSButton *button in _mockButtons) {
        [buttonRow addArrangedSubview:button];
    }

    [self addSubview:buttonRow];

    [NSLayoutConstraint activateConstraints:@[
        [buttonRow.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:8.0],
        [buttonRow.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-8.0],
        [buttonRow.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [buttonRow.heightAnchor constraintEqualToConstant:34.0],
    ]];

    return self;
}

- (void)setButtonsEnabled:(BOOL)enabled {
    for (NSButton *button in _mockButtons) {
        button.enabled = enabled;
    }
}

- (void)handleMockButtonPress:(NSButton *)sender {
    if ([self.delegate respondsToSelector:@selector(homeHeaderView:didTriggerAction:)]) {
        [self.delegate homeHeaderView:self didTriggerAction:(HomeHeaderActionV2)sender.tag];
    }
}

@end

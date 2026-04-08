#import "MainTabView.hpp"

#import "PBDrawableButton.hpp"

@implementation MainTabView {
    NSArray<NSString *> *_segments;
    NSStackView *_buttonsStackView;
    NSMutableArray<PBDrawableButton *> *_segmentButtons;
}

- (instancetype)initWithSegments:(NSArray<NSString *> *)segments {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithWhite:1.0 alpha:0.12].CGColor;
    self.layer.cornerRadius = 10.0;

    _segments = [segments copy] ?: @[];
    _segmentButtons = [NSMutableArray arrayWithCapacity:_segments.count];
    _segmentHeight = 42.0;
    _backgroundColorUnselected = [NSColor colorWithRed:0.31 green:0.31 blue:0.32 alpha:1.0];
    _backgroundColorUnselectedDepressed = [NSColor colorWithRed:0.24 green:0.24 blue:0.25 alpha:1.0];
    _backgroundColorSelected = [NSColor colorWithRed:0.16 green:0.45 blue:0.27 alpha:1.0];
    _backgroundColorSelectedDepressed = [NSColor colorWithRed:0.12 green:0.35 blue:0.20 alpha:1.0];

    _buttonsStackView = [[NSStackView alloc] initWithFrame:NSZeroRect];
    _buttonsStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _buttonsStackView.orientation = NSUserInterfaceLayoutOrientationHorizontal;
    _buttonsStackView.alignment = NSLayoutAttributeCenterY;
    _buttonsStackView.distribution = NSStackViewDistributionFillEqually;
    _buttonsStackView.spacing = 6.0;
    [self addSubview:_buttonsStackView];

    for (NSInteger index = 0; index < (NSInteger)_segments.count; ++index) {
        PBDrawableButton *button = [[PBDrawableButton alloc] initWithFrame:NSZeroRect];
        button.title = _segments[(NSUInteger)index] ?: @"";
        button.font = [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold];
        button.preferredHeight = _segmentHeight;
        button.cornerRadius = 9.0;
        button.borderWidth = 1.0;
        button.borderColor = [NSColor colorWithRed:0.20 green:0.20 blue:0.20 alpha:1.0];
        button.pressedBorderColor = button.borderColor;
        button.selectedBorderColor = [NSColor colorWithRed:0.10 green:0.28 blue:0.16 alpha:1.0];
        button.selectedPressedBorderColor = button.selectedBorderColor;
        button.target = self;
        button.action = @selector(handleSegmentPress:);
        button.tag = index;
        [_segmentButtons addObject:button];
        [_buttonsStackView addArrangedSubview:button];
    }

    [NSLayoutConstraint activateConstraints:@[
        [_buttonsStackView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:8.0],
        [_buttonsStackView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-8.0],
        [_buttonsStackView.topAnchor constraintEqualToAnchor:self.topAnchor constant:8.0],
        [_buttonsStackView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor constant:-8.0],
    ]];

    self.selectedSegment = 0;
    return self;
}

- (NSInteger)segmentCount {
    return (NSInteger)_segments.count;
}

- (void)setEnabled:(BOOL)enabled {
    [super setEnabled:enabled];
    for (PBDrawableButton *button in _segmentButtons) {
        button.enabled = enabled;
    }
}

- (void)setSegmentHeight:(CGFloat)segmentHeight {
    _segmentHeight = MAX(28.0, segmentHeight);
    for (PBDrawableButton *button in _segmentButtons) {
        button.preferredHeight = _segmentHeight;
    }
}

- (void)setSelectedSegment:(NSInteger)selectedSegment {
    NSInteger clamped = selectedSegment;
    if (clamped < 0 || clamped >= self.segmentCount) {
        clamped = 0;
    }
    _selectedSegment = clamped;
    [self refreshButtonStyles];
}

- (NSString *)labelForSegment:(NSInteger)segment {
    if (segment < 0 || segment >= self.segmentCount) {
        return @"";
    }
    return _segments[(NSUInteger)segment] ?: @"";
}

- (void)setBackgroundColorUnselected:(NSColor *)backgroundColorUnselected {
    _backgroundColorUnselected = backgroundColorUnselected;
    [self refreshButtonStyles];
}

- (void)setBackgroundColorUnselectedDepressed:(NSColor *)backgroundColorUnselectedDepressed {
    _backgroundColorUnselectedDepressed = backgroundColorUnselectedDepressed;
    [self refreshButtonStyles];
}

- (void)setBackgroundColorSelected:(NSColor *)backgroundColorSelected {
    _backgroundColorSelected = backgroundColorSelected;
    [self refreshButtonStyles];
}

- (void)setBackgroundColorSelectedDepressed:(NSColor *)backgroundColorSelectedDepressed {
    _backgroundColorSelectedDepressed = backgroundColorSelectedDepressed;
    [self refreshButtonStyles];
}

- (void)refreshButtonStyles {
    for (NSInteger index = 0; index < (NSInteger)_segmentButtons.count; ++index) {
        PBDrawableButton *button = _segmentButtons[(NSUInteger)index];
        BOOL selected = (index == self.selectedSegment);
        button.selected = selected;
        button.backgroundColor = self.backgroundColorUnselected;
        button.pressedBackgroundColor = self.backgroundColorUnselectedDepressed;
        button.selectedBackgroundColor = self.backgroundColorSelected;
        button.selectedPressedBackgroundColor = self.backgroundColorSelectedDepressed;
    }
}

- (void)handleSegmentPress:(PBDrawableButton *)sender {
    if (sender == nil) {
        return;
    }
    self.selectedSegment = sender.tag;
    [self sendAction:self.action to:self.target];
}

@end

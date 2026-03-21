#import "ToolPanelView.hpp"

#import "../UIConstants.hpp"
#import "ToolRowSectionView.hpp"

@implementation ToolPanelView {
    ToolRowSectionView *_backgroundView;
    NSStackView *_rowsStackView;
}

+ (CGFloat)requiredHeightForRowCount:(NSUInteger)rowCount {
    if (rowCount == 0) {
        return kUIPanelPaddingTop + kUIPanelPaddingBottom;
    }

    return (kUIToolRowHeight * rowCount)
        + (kUIPanelSpacingV * (rowCount - 1))
        + kUIPanelPaddingTop
        + kUIPanelPaddingBottom;
}

- (instancetype)initWithBackgroundColor:(NSColor *)backgroundColor {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;

    _backgroundView = [[ToolRowSectionView alloc] initWithBackgroundColor:backgroundColor];
    _rowsStackView = [[NSStackView alloc] initWithFrame:NSZeroRect];
    _rowsStackView.translatesAutoresizingMaskIntoConstraints = NO;
    _rowsStackView.orientation = NSUserInterfaceLayoutOrientationVertical;
    _rowsStackView.alignment = NSLayoutAttributeWidth;
    _rowsStackView.distribution = NSStackViewDistributionFillEqually;
    _rowsStackView.spacing = kUIPanelSpacingV;

    [_backgroundView addSubview:_rowsStackView];
    [self addSubview:_backgroundView];

    [NSLayoutConstraint activateConstraints:@[
        [_backgroundView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [_backgroundView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [_backgroundView.topAnchor constraintEqualToAnchor:self.topAnchor],
        [_backgroundView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],

        [_rowsStackView.leadingAnchor constraintEqualToAnchor:_backgroundView.leadingAnchor constant:kUIPanelPaddingLeft],
        [_rowsStackView.trailingAnchor constraintEqualToAnchor:_backgroundView.trailingAnchor constant:-kUIPanelPaddingRight],
        [_rowsStackView.topAnchor constraintEqualToAnchor:_backgroundView.topAnchor constant:kUIPanelPaddingTop],
        [_rowsStackView.bottomAnchor constraintEqualToAnchor:_backgroundView.bottomAnchor constant:-kUIPanelPaddingBottom],
    ]];

    return self;
}

- (void)addRowViews:(NSArray<NSView *> *)rowViews {
    for (NSView *rowView in rowViews) {
        rowView.translatesAutoresizingMaskIntoConstraints = NO;
        [_rowsStackView addArrangedSubview:rowView];
        [rowView.heightAnchor constraintEqualToConstant:kUIToolRowHeight].active = YES;
    }
}

@end

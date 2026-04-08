#import "ToolBarComboBoxChunkView.hpp"

#import "../UIConstants.hpp"

@implementation ToolBarComboBoxChunkView

- (instancetype)initWithItems:(NSArray<NSString *> *)items {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;

    _comboBox = [[NSPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    _comboBox.translatesAutoresizingMaskIntoConstraints = NO;
    _comboBox.font = [NSFont systemFontOfSize:12.0];
    [_comboBox addItemsWithTitles:(items ?: @[])];
    [self addSubview:_comboBox];

    self.constraintLeft = [_comboBox.leadingAnchor constraintEqualToAnchor:self.leadingAnchor];
    self.constraintRight = [_comboBox.trailingAnchor constraintEqualToAnchor:self.trailingAnchor];

    [NSLayoutConstraint activateConstraints:@[
        self.constraintLeft,
        self.constraintRight,
        [_comboBox.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [_comboBox.heightAnchor constraintEqualToConstant:30.0],
        [self.heightAnchor constraintEqualToConstant:kUIToolRowHeight],
    ]];

    return self;
}

- (void)setSmallPaddingLeft {
    self.constraintLeft.constant = 0.0;
}

- (void)setSmallPaddingRight {
    self.constraintRight.constant = 0.0;
}

- (void)setMediumPaddingRight {
    self.constraintRight.constant = 0.0;
}

@end

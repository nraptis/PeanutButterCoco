#import "ToolBarButtonChunkView.hpp"

#import "../UIConstants.hpp"

@implementation ToolBarButtonChunkView

- (instancetype)initWithTitle:(NSString *)title {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;

    _button = [NSButton buttonWithTitle:(title ?: @"Button") target:nil action:nil];
    _button.translatesAutoresizingMaskIntoConstraints = NO;
    _button.bezelStyle = NSBezelStyleRounded;
    _button.controlSize = NSControlSizeRegular;
    [self addSubview:_button];

    self.constraintLeft = [_button.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:6.0];
    self.constraintRight = [_button.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-6.0];

    [NSLayoutConstraint activateConstraints:@[
        self.constraintLeft,
        self.constraintRight,
        [_button.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [_button.heightAnchor constraintEqualToConstant:32.0],
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
    self.constraintRight.constant = -3.0;
}

@end

#import "MainActionButton.hpp"

#import "../UIConstants.hpp"

@implementation MainActionButton

- (instancetype)initWithTitle:(NSString *)title {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;

    _button = [NSButton buttonWithTitle:(title ?: @"Action") target:nil action:nil];
    _button.translatesAutoresizingMaskIntoConstraints = NO;
    _button.bezelStyle = NSBezelStyleRounded;
    _button.controlSize = NSControlSizeLarge;
    _button.font = [NSFont boldSystemFontOfSize:15.0];
    [self addSubview:_button];

    [NSLayoutConstraint activateConstraints:@[
        [_button.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [_button.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [_button.topAnchor constraintEqualToAnchor:self.topAnchor],
        [_button.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
        [_button.heightAnchor constraintGreaterThanOrEqualToConstant:(kUIToolRowElementHeight - 4.0)],
    ]];

    return self;
}

@end

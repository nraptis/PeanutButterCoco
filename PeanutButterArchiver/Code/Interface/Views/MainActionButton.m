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
    _button.bezelStyle = NSBezelStyleRegularSquare;
    _button.bordered = NO;
    _button.wantsLayer = YES;
    _button.layer.backgroundColor = [NSColor colorWithRed:0.13 green:0.39 blue:0.17 alpha:1.0].CGColor;
    _button.layer.cornerRadius = 12.0;
    _button.font = [NSFont boldSystemFontOfSize:16.0];
    _button.contentTintColor = NSColor.whiteColor;
    [self addSubview:_button];

    [NSLayoutConstraint activateConstraints:@[
        [_button.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [_button.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [_button.topAnchor constraintEqualToAnchor:self.topAnchor],
        [_button.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];

    return self;
}

@end

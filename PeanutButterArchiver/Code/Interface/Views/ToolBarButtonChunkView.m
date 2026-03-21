#import "ToolBarButtonChunkView.hpp"

#import "../UIConstants.hpp"

@implementation ToolBarButtonChunkView

- (instancetype)initWithTitle:(NSString *)title {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:1.0 green:0.0 blue:1.0 alpha:0.25].CGColor;
    self.layer.cornerRadius = 8.0;
    _button = [NSButton buttonWithTitle:(title ?: @"Button") target:nil action:nil];
    _button.translatesAutoresizingMaskIntoConstraints = NO;
    _button.bezelStyle = NSBezelStyleShadowlessSquare;
    [self addSubview:_button];

    [NSLayoutConstraint activateConstraints:@[
        [_button.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:0.0],
        [_button.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:0.0],
        [_button.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [_button.heightAnchor constraintEqualToConstant:kUIToolRowElementHeight],
        [self.heightAnchor constraintEqualToConstant:kUIToolRowHeight],
    ]];

    return self;
}

@end

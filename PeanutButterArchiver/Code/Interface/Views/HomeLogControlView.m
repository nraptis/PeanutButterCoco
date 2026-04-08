#import "HomeLogControlView.hpp"

#import "../UIConstants.hpp"
#import "PBDrawableButton.hpp"

@implementation HomeLogControlView

@synthesize clearLogsButton = _clearLogsButton;
@synthesize scrollToBottomButton = _scrollToBottomButton;
@synthesize verboseEventsButton = _verboseEventsButton;

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:0.72 green:0.67 blue:0.59 alpha:1.0].CGColor;
    self.layer.cornerRadius = 8.0;

    _clearLogsButton = [self buildButtonWithTitle:@"Clear Logs"];
    _scrollToBottomButton = [self buildButtonWithTitle:@"Scroll to Bottom"];
    _verboseEventsButton = [self buildButtonWithTitle:@"Verbose Events"];
    _verboseEventsButton.togglesOnClick = YES;
    _verboseEventsButton.backgroundColor = [NSColor colorWithRed:0.34 green:0.34 blue:0.35 alpha:1.0];
    _verboseEventsButton.pressedBackgroundColor = [NSColor colorWithRed:0.27 green:0.27 blue:0.28 alpha:1.0];
    _verboseEventsButton.selectedBackgroundColor = [NSColor colorWithRed:0.23 green:0.48 blue:0.20 alpha:1.0];
    _verboseEventsButton.selectedPressedBackgroundColor = [NSColor colorWithRed:0.17 green:0.37 blue:0.15 alpha:1.0];

    for (PBDrawableButton *button in @[_clearLogsButton, _scrollToBottomButton, _verboseEventsButton]) {
        [self addSubview:button];
        [button.centerYAnchor constraintEqualToAnchor:self.centerYAnchor].active = YES;
    }

    [NSLayoutConstraint activateConstraints:@[
        [_clearLogsButton.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kMiddleToolbarPaddingLeft],
        [_clearLogsButton.widthAnchor constraintEqualToConstant:kClearLogsButtonWidth],
        [_clearLogsButton.heightAnchor constraintEqualToConstant:34.0],

        [_scrollToBottomButton.leadingAnchor constraintEqualToAnchor:_clearLogsButton.trailingAnchor constant:kMiddleToolbarSpacingH],
        [_scrollToBottomButton.widthAnchor constraintEqualToConstant:kScrollToBottomButtonWidth],
        [_scrollToBottomButton.heightAnchor constraintEqualToConstant:34.0],

        [_verboseEventsButton.leadingAnchor constraintEqualToAnchor:_scrollToBottomButton.trailingAnchor constant:kMiddleToolbarSpacingH],
        [_verboseEventsButton.widthAnchor constraintEqualToConstant:144.0],
        [_verboseEventsButton.heightAnchor constraintEqualToConstant:34.0],
        [_verboseEventsButton.trailingAnchor constraintLessThanOrEqualToAnchor:self.trailingAnchor constant:-kMiddleToolbarPaddingLeft],
    ]];

    return self;
}

- (PBDrawableButton *)buildButtonWithTitle:(NSString *)title {
    PBDrawableButton *button = [[PBDrawableButton alloc] initWithFrame:NSZeroRect];
    button.title = title ?: @"Button";
    button.font = [NSFont systemFontOfSize:12.0 weight:NSFontWeightSemibold];
    button.preferredHeight = 34.0;
    button.cornerRadius = 9.0;
    button.contentInsets = NSEdgeInsetsMake(6.0, 12.0, 6.0, 12.0);
    button.backgroundColor = [NSColor colorWithRed:0.36 green:0.36 blue:0.36 alpha:1.0];
    button.pressedBackgroundColor = [NSColor colorWithRed:0.28 green:0.28 blue:0.29 alpha:1.0];
    button.selectedBackgroundColor = button.backgroundColor;
    button.selectedPressedBackgroundColor = button.pressedBackgroundColor;
    button.borderColor = [NSColor colorWithRed:0.22 green:0.20 blue:0.18 alpha:1.0];
    button.pressedBorderColor = button.borderColor;
    button.selectedBorderColor = button.borderColor;
    button.selectedPressedBorderColor = button.borderColor;
    return button;
}

@end

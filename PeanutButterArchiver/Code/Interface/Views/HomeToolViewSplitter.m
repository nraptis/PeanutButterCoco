#import "HomeToolViewSplitter.hpp"

#import "../UIConstants.hpp"

@implementation HomeToolViewSplitter

@synthesize clearLogsButton = _clearLogsButton;
@synthesize scrollToBottomButton = _scrollToBottomButton;

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:0.72 green:0.67 blue:0.59 alpha:1.0].CGColor;
    self.layer.cornerRadius = 6.0;

    _clearLogsButton = [NSButton buttonWithTitle:@"Clear Logs" target:nil action:nil];
    _scrollToBottomButton = [NSButton buttonWithTitle:@"Scroll To Bottom" target:nil action:nil];

    for (NSButton *button in @[_clearLogsButton, _scrollToBottomButton]) {
        button.translatesAutoresizingMaskIntoConstraints = NO;
        [self addSubview:button];
        [button.centerYAnchor constraintEqualToAnchor:self.centerYAnchor].active = YES;
    }

    [NSLayoutConstraint activateConstraints:@[
        [_clearLogsButton.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kMiddleToolbarPaddingLeft],
        [_clearLogsButton.widthAnchor constraintEqualToConstant:kClearLogsButtonWidth],
        [_clearLogsButton.heightAnchor constraintEqualToConstant:kClearLogsButtonHeight],

        [_scrollToBottomButton.leadingAnchor constraintEqualToAnchor:_clearLogsButton.trailingAnchor constant:kMiddleToolbarSpacingH],
        [_scrollToBottomButton.widthAnchor constraintEqualToConstant:kScrollToBottomButtonWidth],
        [_scrollToBottomButton.heightAnchor constraintEqualToConstant:kScrollToBottomButtonHeight],
    ]];
    return self;
}

@end

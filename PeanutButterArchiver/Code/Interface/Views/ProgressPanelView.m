#import "ProgressPanelView.hpp"

#import "../UIConstants.hpp"

@implementation ProgressPanelView

@synthesize cancelButton = _cancelButton;

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:0.95 green:0.90 blue:0.82 alpha:1.0].CGColor;
    self.layer.cornerRadius = kUICardCornerRadius;

    self.titleLabel = [NSTextField labelWithString:@"ProgressPanelView"];
    self.titleLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.titleLabel.font = [NSFont boldSystemFontOfSize:24.0];

    self.detailLabel = [NSTextField labelWithString:@""];
    self.detailLabel.translatesAutoresizingMaskIntoConstraints = NO;
    self.detailLabel.font = [NSFont systemFontOfSize:14.0];
    self.detailLabel.maximumNumberOfLines = 2;
    self.detailLabel.hidden = YES;

    self.progressIndicator = [[NSProgressIndicator alloc] initWithFrame:NSZeroRect];
    self.progressIndicator.translatesAutoresizingMaskIntoConstraints = NO;
    self.progressIndicator.style = NSProgressIndicatorStyleBar;
    self.progressIndicator.indeterminate = NO;
    self.progressIndicator.usesThreadedAnimation = NO;
    self.progressIndicator.minValue = 0.0;
    self.progressIndicator.maxValue = 100.0;
    self.progressIndicator.doubleValue = 0.0;
    self.progressIndicator.controlSize = NSControlSizeLarge;

    _cancelButton = [NSButton buttonWithTitle:@"Cancel" target:nil action:nil];
    _cancelButton.translatesAutoresizingMaskIntoConstraints = NO;
    _cancelButton.enabled = NO;
    _cancelButton.bezelStyle = NSBezelStyleRounded;
    _cancelButton.controlSize = NSControlSizeRegular;

    [self addSubview:self.titleLabel];
    [self addSubview:self.detailLabel];
    [self addSubview:self.progressIndicator];
    [self addSubview:_cancelButton];

    [NSLayoutConstraint activateConstraints:@[
        [self.titleLabel.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:kUIMakeArchiveContentInset],
        [self.titleLabel.topAnchor constraintEqualToAnchor:self.topAnchor constant:kUIMakeArchiveContentInset],
        [self.progressIndicator.leadingAnchor constraintEqualToAnchor:self.titleLabel.leadingAnchor],
        [self.progressIndicator.trailingAnchor constraintEqualToAnchor:_cancelButton.leadingAnchor constant:-kMiddleToolbarSpacingH],
        [self.progressIndicator.topAnchor constraintEqualToAnchor:self.titleLabel.bottomAnchor constant:18.0],
        [self.progressIndicator.heightAnchor constraintEqualToConstant:28.0],
        [_cancelButton.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-kUIMakeArchiveContentInset],
        [_cancelButton.centerYAnchor constraintEqualToAnchor:self.progressIndicator.centerYAnchor],
        [_cancelButton.widthAnchor constraintEqualToConstant:kCancelButtonWidth],
        [_cancelButton.heightAnchor constraintEqualToConstant:30.0],
    ]];

    return self;
}

@end

#import "ToolBarCheckBoxChunkView.hpp"

#import "../UIConstants.hpp"

@implementation ToolBarCheckBoxChunkView

- (instancetype)initWithTitle:(NSString *)title
                        state:(BOOL)state {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    //self.wantsLayer = YES;
    //self.layer.backgroundColor = [NSColor colorWithWhite:1.0 alpha:0.18].CGColor;
    //self.layer.cornerRadius = 8.0;
    
    _checkBox = [NSButton checkboxWithTitle:(title ?: @"")
                                     target:nil
                                     action:nil];
    _checkBox.translatesAutoresizingMaskIntoConstraints = NO;
    _checkBox.state = state ? NSControlStateValueOn : NSControlStateValueOff;
    _checkBox.alignment = NSTextAlignmentLeft;
    [self addSubview:_checkBox];

    [NSLayoutConstraint activateConstraints:@[
        [_checkBox.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:12.0],
        [_checkBox.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-4.0],
        [_checkBox.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [_checkBox.heightAnchor constraintEqualToConstant:kUIToolRowElementHeight],
        [self.heightAnchor constraintEqualToConstant:kUIToolRowHeight],
    ]];

    [self setContentHuggingPriority:NSLayoutPriorityDefaultLow
                     forOrientation:NSLayoutConstraintOrientationHorizontal];
    [self setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                   forOrientation:NSLayoutConstraintOrientationHorizontal];

    return self;
}

@end

@implementation ToolBarCheckBoxRightAlignedChunkView

- (instancetype)initWithTitle:(NSString *)title
                        state:(BOOL)state {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    //self.wantsLayer = YES;
    //self.layer.backgroundColor = [NSColor colorWithWhite:1.0 alpha:0.18].CGColor;
    //self.layer.cornerRadius = 8.0;
    
    _checkBox = [NSButton checkboxWithTitle:(title ?: @"")
                                     target:nil
                                     action:nil];
    _checkBox.translatesAutoresizingMaskIntoConstraints = NO;
    _checkBox.state = state ? NSControlStateValueOn : NSControlStateValueOff;
    _checkBox.alignment = NSTextAlignmentRight;
    _checkBox.imagePosition = NSImageRight;
    [self addSubview:_checkBox];

    [NSLayoutConstraint activateConstraints:@[
        [_checkBox.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:4.0],
        [_checkBox.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:-12.0],
        [_checkBox.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [_checkBox.heightAnchor constraintEqualToConstant:kUIToolRowElementHeight],
        [self.heightAnchor constraintEqualToConstant:kUIToolRowHeight],
    ]];

    [self setContentHuggingPriority:NSLayoutPriorityDefaultLow
                     forOrientation:NSLayoutConstraintOrientationHorizontal];
    [self setContentCompressionResistancePriority:NSLayoutPriorityDefaultLow
                                   forOrientation:NSLayoutConstraintOrientationHorizontal];

    return self;
}

@end

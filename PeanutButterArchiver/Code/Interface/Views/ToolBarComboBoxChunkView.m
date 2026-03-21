#import "ToolBarComboBoxChunkView.hpp"

#import "../UIConstants.hpp"

@interface PBStretchPopUpButton : NSPopUpButton
@end

@implementation PBStretchPopUpButton

- (NSSize)intrinsicContentSize {
    NSSize size = [super intrinsicContentSize];
    size.width = NSViewNoIntrinsicMetric;
    return size;
}

@end

@implementation ToolBarComboBoxChunkView

- (instancetype)initWithItems:(NSArray<NSString *> *)items {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:1.0 green:0.0 blue:0.0 alpha:0.25].CGColor;
    self.layer.cornerRadius = 8.0;
    _comboBox = [[PBStretchPopUpButton alloc] initWithFrame:NSZeroRect pullsDown:NO];
    _comboBox.translatesAutoresizingMaskIntoConstraints = NO;
    if ([_comboBox.cell respondsToSelector:@selector(setBezelStyle:)]) {
        [(NSPopUpButtonCell *)_comboBox.cell setBezelStyle:NSBezelStyleSmallSquare];
    }
    [_comboBox addItemsWithTitles:(items ?: @[])];
    [self addSubview:_comboBox];

    [NSLayoutConstraint activateConstraints:@[
        [_comboBox.leadingAnchor constraintEqualToAnchor:self.leadingAnchor constant:0.0],
        [_comboBox.trailingAnchor constraintEqualToAnchor:self.trailingAnchor constant:0.0],
        [_comboBox.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [_comboBox.heightAnchor constraintEqualToConstant:kUIToolRowElementHeight],
        [self.heightAnchor constraintEqualToConstant:kUIToolRowHeight],
    ]];

    return self;
}

@end

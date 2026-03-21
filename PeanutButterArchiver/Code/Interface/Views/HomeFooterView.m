#import "HomeFooterView.hpp"

@implementation HomeFooterView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:0.23 green:0.74 blue:0.25 alpha:1.0].CGColor;
    self.layer.cornerRadius = 4.0;

    return self;
}

@end

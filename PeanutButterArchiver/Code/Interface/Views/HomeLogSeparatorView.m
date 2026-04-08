#import "HomeLogSeparatorView.hpp"

@implementation HomeLogSeparatorView

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    return self;
}

- (BOOL)isFlipped {
    return YES;
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];
    [[NSBezierPath bezierPathWithRect:self.bounds] addClip];

    [[NSColor colorWithRed:0.52 green:0.48 blue:0.41 alpha:1.0] setFill];
    NSRectFill(self.bounds);

    [[NSColor colorWithWhite:1.0 alpha:0.15] setFill];
    NSRect highlightRect = NSMakeRect(0.0, 0.0, NSWidth(self.bounds), 1.0);
    NSRectFill(highlightRect);

    [[NSColor colorWithRed:0.29 green:0.27 blue:0.23 alpha:1.0] setFill];
    NSRect lineRect = NSMakeRect(0.0, floor((NSHeight(self.bounds) - 2.0) * 0.5), NSWidth(self.bounds), 2.0);
    NSRectFill(lineRect);
}

@end

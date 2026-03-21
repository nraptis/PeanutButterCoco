#import "ToolRowSectionView.hpp"

#import "../UIConstants.hpp"

@implementation ToolRowSectionView

- (instancetype)initWithBackgroundColor:(NSColor *)backgroundColor {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.wantsLayer = YES;
    self.layer.cornerRadius = kUISubcardCornerRadius;
    [self setBackgroundColor:backgroundColor];
    return self;
}

- (void)setBackgroundColor:(NSColor *)backgroundColor {
    self.layer.backgroundColor = (backgroundColor ?: NSColor.clearColor).CGColor;
}

@end

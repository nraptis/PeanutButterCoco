#import "PBDrawableButton.hpp"

static NSColor *PBColorWithPressedFallback(NSColor *preferred, NSColor *fallback) {
    return preferred ?: fallback ?: [NSColor clearColor];
}

static NSColor *PBDisabledColor(NSColor *color) {
    return [(color ?: [NSColor clearColor]) colorWithAlphaComponent:0.45];
}

@implementation PBDrawableButton {
    BOOL _pressed;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    _title = @"";
    [super setFont:[NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold]];
    _titleColor = [NSColor colorWithRed:0.95 green:0.94 blue:0.91 alpha:1.0];
    _disabledTitleColor = [NSColor colorWithWhite:0.85 alpha:0.6];
    _backgroundColor = [NSColor colorWithRed:0.33 green:0.33 blue:0.34 alpha:1.0];
    _pressedBackgroundColor = [NSColor colorWithRed:0.27 green:0.27 blue:0.28 alpha:1.0];
    _selectedBackgroundColor = _backgroundColor;
    _selectedPressedBackgroundColor = _pressedBackgroundColor;
    _borderColor = [NSColor colorWithRed:0.23 green:0.21 blue:0.19 alpha:1.0];
    _pressedBorderColor = _borderColor;
    _selectedBorderColor = _borderColor;
    _selectedPressedBorderColor = _borderColor;
    _borderWidth = 1.0;
    _cornerRadius = 8.0;
    _preferredHeight = 36.0;
    _contentInsets = NSEdgeInsetsMake(6.0, 12.0, 6.0, 12.0);
    self.wantsLayer = YES;

    return self;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)acceptsFirstResponder {
    return NO;
}

- (void)setEnabled:(BOOL)enabled {
    [super setEnabled:enabled];
    [self setNeedsDisplay:YES];
}

- (void)setTitle:(NSString *)title {
    _title = [title copy] ?: @"";
    [self invalidateIntrinsicContentSize];
    [self setNeedsDisplay:YES];
}

- (void)setFont:(NSFont *)font {
    [super setFont:(font ?: [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold])];
    [self invalidateIntrinsicContentSize];
    [self setNeedsDisplay:YES];
}

- (void)setSelected:(BOOL)selected {
    _selected = selected;
    [self setNeedsDisplay:YES];
}

- (void)setPreferredHeight:(CGFloat)preferredHeight {
    _preferredHeight = MAX(24.0, preferredHeight);
    [self invalidateIntrinsicContentSize];
    [self setNeedsDisplay:YES];
}

- (void)setContentInsets:(NSEdgeInsets)contentInsets {
    _contentInsets = contentInsets;
    [self invalidateIntrinsicContentSize];
    [self setNeedsDisplay:YES];
}

- (NSSize)intrinsicContentSize {
    NSDictionary<NSAttributedStringKey, id> *attributes = @{
        NSFontAttributeName: self.font ?: [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold],
    };
    NSSize titleSize = [self.title ?: @"" sizeWithAttributes:attributes];
    return NSMakeSize(ceil(titleSize.width + self.contentInsets.left + self.contentInsets.right),
                      ceil(MAX(titleSize.height + self.contentInsets.top + self.contentInsets.bottom,
                               self.preferredHeight)));
}

- (NSColor *)activeBackgroundColor {
    if (!self.enabled) {
        return PBDisabledColor(self.isSelected ? PBColorWithPressedFallback(self.selectedBackgroundColor, self.backgroundColor)
                                               : self.backgroundColor);
    }
    if (self.isSelected) {
        return _pressed ? PBColorWithPressedFallback(self.selectedPressedBackgroundColor, self.selectedBackgroundColor)
                        : PBColorWithPressedFallback(self.selectedBackgroundColor, self.backgroundColor);
    }
    return _pressed ? PBColorWithPressedFallback(self.pressedBackgroundColor, self.backgroundColor)
                    : PBColorWithPressedFallback(self.backgroundColor, [NSColor clearColor]);
}

- (NSColor *)activeBorderColor {
    if (!self.enabled) {
        return PBDisabledColor(self.isSelected ? PBColorWithPressedFallback(self.selectedBorderColor, self.borderColor)
                                               : self.borderColor);
    }
    if (self.isSelected) {
        return _pressed ? PBColorWithPressedFallback(self.selectedPressedBorderColor, self.selectedBorderColor)
                        : PBColorWithPressedFallback(self.selectedBorderColor, self.borderColor);
    }
    return _pressed ? PBColorWithPressedFallback(self.pressedBorderColor, self.borderColor)
                    : PBColorWithPressedFallback(self.borderColor, [NSColor clearColor]);
}

- (void)drawRect:(NSRect)dirtyRect {
    [super drawRect:dirtyRect];

    NSBezierPath *path = [NSBezierPath bezierPathWithRoundedRect:NSInsetRect(self.bounds, 0.5, 0.5)
                                                         xRadius:self.cornerRadius
                                                         yRadius:self.cornerRadius];
    [[self activeBackgroundColor] setFill];
    [path fill];

    if (self.borderWidth > 0.0) {
        [path setLineWidth:self.borderWidth];
        [[self activeBorderColor] setStroke];
        [path stroke];
    }

    NSMutableParagraphStyle *paragraphStyle = [[NSMutableParagraphStyle alloc] init];
    paragraphStyle.alignment = NSTextAlignmentCenter;
    paragraphStyle.lineBreakMode = NSLineBreakByTruncatingTail;

    NSDictionary<NSAttributedStringKey, id> *attributes = @{
        NSFontAttributeName: self.font ?: [NSFont systemFontOfSize:13.0 weight:NSFontWeightSemibold],
        NSForegroundColorAttributeName: (self.enabled ? (self.titleColor ?: NSColor.whiteColor)
                                                      : (self.disabledTitleColor ?: PBDisabledColor(self.titleColor))),
        NSParagraphStyleAttributeName: paragraphStyle,
    };
    NSRect titleBounds = NSInsetRect(self.bounds, self.contentInsets.left, self.contentInsets.top);
    titleBounds.size.width -= self.contentInsets.right;
    titleBounds.size.height -= self.contentInsets.bottom;
    NSSize titleSize = [self.title ?: @"" sizeWithAttributes:attributes];
    NSRect drawRect = NSMakeRect(NSMinX(titleBounds),
                                 NSMidY(self.bounds) - (titleSize.height * 0.5),
                                 NSWidth(titleBounds),
                                 titleSize.height);
    [self.title ?: @"" drawInRect:NSIntegralRect(drawRect) withAttributes:attributes];
}

- (BOOL)pointInside:(NSPoint)point {
    return NSPointInRect(point, self.bounds);
}

- (void)mouseDown:(NSEvent *)event {
    if (!self.enabled) {
        return;
    }

    _pressed = YES;
    [self setNeedsDisplay:YES];

    while (YES) {
        NSEvent *nextEvent =
            [self.window nextEventMatchingMask:(NSEventMaskLeftMouseDragged | NSEventMaskLeftMouseUp)];
        NSPoint point = [self convertPoint:nextEvent.locationInWindow fromView:nil];
        BOOL inside = [self pointInside:point];
        if (_pressed != inside) {
            _pressed = inside;
            [self setNeedsDisplay:YES];
        }

        if (nextEvent.type == NSEventTypeLeftMouseUp) {
            if (inside) {
                if (self.togglesOnClick) {
                    self.selected = !self.selected;
                }
                [self sendAction:self.action to:self.target];
            }
            break;
        }
    }

    _pressed = NO;
    [self setNeedsDisplay:YES];
}

@end

#import "HomeLogView.hpp"

#import "../UIConstants.hpp"

@implementation HomeLogView {
    NSScrollView *_scrollView;
}

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;
    self.wantsLayer = YES;
    self.layer.backgroundColor = [NSColor colorWithRed:0.96 green:0.88 blue:0.38 alpha:1.0].CGColor;
    self.layer.cornerRadius = kUISubcardCornerRadius;

    _scrollView = [[NSScrollView alloc] initWithFrame:NSZeroRect];
    _scrollView.translatesAutoresizingMaskIntoConstraints = NO;
    _scrollView.hasVerticalScroller = YES;
    _scrollView.drawsBackground = NO;
    _scrollView.borderType = NSNoBorder;

    NSTextStorage *textStorage = [[NSTextStorage alloc] init];
    NSLayoutManager *layoutManager = [[NSLayoutManager alloc] init];
    NSTextContainer *textContainer = [[NSTextContainer alloc] initWithContainerSize:NSMakeSize(0.0, CGFLOAT_MAX)];
    textContainer.widthTracksTextView = YES;
    [layoutManager addTextContainer:textContainer];
    [textStorage addLayoutManager:layoutManager];

    _textView = [[NSTextView alloc] initWithFrame:NSZeroRect textContainer:textContainer];
    _textView.editable = NO;
    _textView.richText = NO;
    _textView.importsGraphics = NO;
    _textView.usesFindBar = YES;
    _textView.automaticQuoteSubstitutionEnabled = NO;
    _textView.automaticDashSubstitutionEnabled = NO;
    _textView.automaticTextReplacementEnabled = NO;
    _textView.automaticSpellingCorrectionEnabled = NO;
    _textView.continuousSpellCheckingEnabled = NO;
    _textView.grammarCheckingEnabled = NO;
    _textView.allowsUndo = NO;
    _textView.drawsBackground = NO;
    _textView.verticallyResizable = YES;
    _textView.horizontallyResizable = NO;
    _textView.minSize = NSMakeSize(0.0, kUILogViewHeight);
    _textView.maxSize = NSMakeSize(CGFLOAT_MAX, CGFLOAT_MAX);
    _textView.textContainerInset = NSMakeSize(10.0, 10.0);
    _textView.font = [NSFont monospacedSystemFontOfSize:13.0 weight:NSFontWeightMedium];
    _textView.textColor = [NSColor colorWithRed:0.05 green:0.21 blue:0.76 alpha:1.0];
    _textView.string = @"";

    _scrollView.documentView = _textView;
    [self addSubview:_scrollView];

    [NSLayoutConstraint activateConstraints:@[
        [_scrollView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [_scrollView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [_scrollView.topAnchor constraintEqualToAnchor:self.topAnchor],
        [_scrollView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];

    return self;
}

- (void)appendLine:(NSString *)line {
    if (line.length == 0) {
        return;
    }

    NSTextStorage *textStorage = self.textView.textStorage;
    if (textStorage == nil) {
        return;
    }

    NSClipView *clipView = _scrollView.contentView;
    NSRect visibleRect = clipView.documentVisibleRect;
    NSRect boundsRect = self.textView.bounds;
    BOOL wasPinnedToBottom = NSMaxY(visibleRect) >= (NSMaxY(boundsRect) - 8.0);

    NSMutableAttributedString *appendString = [[NSMutableAttributedString alloc] init];
    if (textStorage.length > 0) {
        [appendString appendAttributedString:[[NSAttributedString alloc] initWithString:@"\n"]];
    }

    NSDictionary<NSAttributedStringKey, id> *attributes = @{
        NSFontAttributeName: self.textView.font ?: [NSFont monospacedSystemFontOfSize:13.0 weight:NSFontWeightMedium],
        NSForegroundColorAttributeName: self.textView.textColor ?: NSColor.blueColor,
    };
    [appendString appendAttributedString:[[NSAttributedString alloc] initWithString:line
                                                                          attributes:attributes]];

    [textStorage beginEditing];
    [textStorage appendAttributedString:appendString];
    [textStorage endEditing];

    [self.textView.layoutManager ensureLayoutForTextContainer:self.textView.textContainer];

    if (wasPinnedToBottom) {
        [self scrollToBottom];
    }
}

- (void)clearAll {
    self.textView.string = @"";
}

- (void)scrollToBottom {
    NSTextStorage *textStorage = self.textView.textStorage;
    if (textStorage == nil) {
        return;
    }
    [self.textView scrollRangeToVisible:NSMakeRange(textStorage.length, 0)];
}

@end

#import "HomeLogView.hpp"

#import "../UIConstants.hpp"

static const NSUInteger kLogCharacterHardLimit = 220000;
static const NSUInteger kLogCharacterSoftLimit = 180000;

@implementation HomeLogView {
    NSScrollView *_scrollView;
    BOOL _autoScrollPinnedToBottom;
    BOOL _isPerformingProgrammaticScroll;
    BOOL _hasDeferredScrollRequest;
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
    _autoScrollPinnedToBottom = YES;
    _isPerformingProgrammaticScroll = NO;
    _hasDeferredScrollRequest = NO;

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
    _scrollView.contentView.postsBoundsChangedNotifications = YES;
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(handleClipViewBoundsDidChange:)
                                                 name:NSViewBoundsDidChangeNotification
                                               object:_scrollView.contentView];
    [self addSubview:_scrollView];

    [NSLayoutConstraint activateConstraints:@[
        [_scrollView.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [_scrollView.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [_scrollView.topAnchor constraintEqualToAnchor:self.topAnchor],
        [_scrollView.bottomAnchor constraintEqualToAnchor:self.bottomAnchor],
    ]];

    return self;
}

- (void)dealloc {
    [[NSNotificationCenter defaultCenter] removeObserver:self
                                                    name:NSViewBoundsDidChangeNotification
                                                  object:_scrollView.contentView];
}

- (void)handleClipViewBoundsDidChange:(NSNotification *)notification {
    (void)notification;
    if (_isPerformingProgrammaticScroll) {
        return;
    }
    _autoScrollPinnedToBottom = [self isNearBottom];
}

- (BOOL)isNearBottom {
    NSClipView *clipView = _scrollView.contentView;
    NSView *documentView = _scrollView.documentView;
    if (clipView == nil || documentView == nil) {
        return YES;
    }

    NSRect visibleRect = clipView.documentVisibleRect;
    NSRect documentRect = documentView.bounds;
    const CGFloat distanceFromBottom = NSMaxY(documentRect) - NSMaxY(visibleRect);
    return distanceFromBottom <= 12.0;
}

- (void)scheduleDeferredScrollToBottomIfNeeded {
    if (_hasDeferredScrollRequest) {
        return;
    }

    _hasDeferredScrollRequest = YES;
    __weak typeof(self) weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        HomeLogView *strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }
        strongSelf->_hasDeferredScrollRequest = NO;
        if (!strongSelf->_autoScrollPinnedToBottom) {
            return;
        }
        [strongSelf scrollToBottom];
    });
}

- (void)appendLine:(NSString *)line {
    if (line.length == 0) {
        return;
    }

    NSTextStorage *textStorage = self.textView.textStorage;
    if (textStorage == nil) {
        return;
    }

    BOOL shouldStickToBottom = _autoScrollPinnedToBottom || [self isNearBottom];

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
    if (textStorage.length > kLogCharacterHardLimit) {
        NSString *fullText = textStorage.string ?: @"";
        NSUInteger trimStart = textStorage.length - kLogCharacterSoftLimit;
        if (trimStart > fullText.length) {
            trimStart = fullText.length;
        }
        NSRange searchRange = NSMakeRange(trimStart, fullText.length - trimStart);
        NSRange newlineRange = [fullText rangeOfString:@"\n"
                                               options:0
                                                 range:searchRange];
        NSUInteger removeLength = (newlineRange.location != NSNotFound)
            ? NSMaxRange(newlineRange)
            : trimStart;
        if (removeLength > textStorage.length) {
            removeLength = textStorage.length;
        }
        if (removeLength > 0) {
            [textStorage deleteCharactersInRange:NSMakeRange(0, removeLength)];
        }
    }
    [textStorage endEditing];

    [self.textView.layoutManager ensureLayoutForTextContainer:self.textView.textContainer];

    if (shouldStickToBottom) {
        [self scrollToBottom];
        [self scheduleDeferredScrollToBottomIfNeeded];
    }
}

- (void)clearAll {
    self.textView.string = @"";
    _autoScrollPinnedToBottom = YES;
}

- (void)scrollToBottom {
    NSTextStorage *textStorage = self.textView.textStorage;
    if (textStorage == nil) {
        return;
    }
    _isPerformingProgrammaticScroll = YES;
    [self.textView scrollRangeToVisible:NSMakeRange(textStorage.length, 0)];
    [_scrollView reflectScrolledClipView:_scrollView.contentView];
    _isPerformingProgrammaticScroll = NO;
    _autoScrollPinnedToBottom = YES;
}

@end

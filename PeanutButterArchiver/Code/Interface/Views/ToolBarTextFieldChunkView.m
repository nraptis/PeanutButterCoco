#import "ToolBarTextFieldChunkView.hpp"

#import "../UIConstants.hpp"

@interface PBPathDroppingTextField : NSTextField

@property (copy, nonatomic) void (^pathDropHandler)(NSString *path);

@end

@implementation PBPathDroppingTextField

- (instancetype)initWithFrame:(NSRect)frameRect {
    self = [super initWithFrame:frameRect];
    if (self != nil) {
        [self registerForDraggedTypes:@[NSPasteboardTypeFileURL]];
        self.font = [NSFont systemFontOfSize:13.0];
        self.focusRingType = NSFocusRingTypeDefault;
        self.bordered = YES;
        self.bezeled = YES;
        self.drawsBackground = YES;
    }
    return self;
}

- (NSArray<NSURL *> *)draggedFileURLsFromPasteboard:(NSPasteboard *)pasteboard {
    return [pasteboard readObjectsForClasses:@[[NSURL class]]
                                     options:@{NSPasteboardURLReadingFileURLsOnlyKey: @YES}];
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
    return ([self draggedFileURLsFromPasteboard:sender.draggingPasteboard].count > 0)
        ? NSDragOperationCopy
        : NSDragOperationNone;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
    NSURL *firstURL = [self draggedFileURLsFromPasteboard:sender.draggingPasteboard].firstObject;
    if (firstURL == nil) {
        return NO;
    }

    NSString *path = firstURL.path ?: @"";
    self.stringValue = path;
    if (self.pathDropHandler != nil) {
        self.pathDropHandler(path);
    }
    return YES;
}

@end

@implementation ToolBarTextFieldChunkView

- (instancetype)initWithFrame:(NSRect)frameRect {
    return [self initWithText:@"" placeholder:@""];
}

- (instancetype)initWithText:(NSString *)text
                 placeholder:(NSString *)placeholder {
    self = [super initWithFrame:NSZeroRect];
    if (self == nil) {
        return nil;
    }

    self.translatesAutoresizingMaskIntoConstraints = NO;

    PBPathDroppingTextField *textField = [[PBPathDroppingTextField alloc] initWithFrame:NSZeroRect];
    _textField = textField;
    _textField.translatesAutoresizingMaskIntoConstraints = NO;
    _textField.stringValue = text ?: @"";
    _textField.placeholderString = placeholder ?: @"";
    [self addSubview:_textField];

    [NSLayoutConstraint activateConstraints:@[
        [_textField.leadingAnchor constraintEqualToAnchor:self.leadingAnchor],
        [_textField.trailingAnchor constraintEqualToAnchor:self.trailingAnchor],
        [_textField.centerYAnchor constraintEqualToAnchor:self.centerYAnchor],
        [_textField.heightAnchor constraintEqualToConstant:30.0],
        [self.heightAnchor constraintEqualToConstant:kUIToolRowHeight],
    ]];

    return self;
}

- (void)setPathDropHandler:(void (^)(NSString *path))pathDropHandler {
    _pathDropHandler = [pathDropHandler copy];
    ((PBPathDroppingTextField *)self.textField).pathDropHandler = _pathDropHandler;
}

@end

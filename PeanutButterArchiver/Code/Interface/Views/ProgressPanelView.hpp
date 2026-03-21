#import <Cocoa/Cocoa.h>

@interface ProgressPanelView : NSView

@property (strong, nonatomic) NSProgressIndicator *progressIndicator;
@property (strong, nonatomic) NSTextField *titleLabel;
@property (strong, nonatomic) NSTextField *detailLabel;
@property (strong, nonatomic, readonly) NSButton *cancelButton;

@end

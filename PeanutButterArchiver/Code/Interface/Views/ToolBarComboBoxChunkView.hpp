#import <Cocoa/Cocoa.h>

@interface ToolBarComboBoxChunkView : NSView

@property (strong, nonatomic, readonly) NSPopUpButton *comboBox;
@property (strong, nonatomic) NSLayoutConstraint *constraintLeft;
@property (strong, nonatomic) NSLayoutConstraint *constraintRight;

- (instancetype)initWithItems:(NSArray<NSString *> *)items;

- (void) setSmallPaddingLeft;
- (void) setSmallPaddingRight;

- (void) setMediumPaddingRight;


@end

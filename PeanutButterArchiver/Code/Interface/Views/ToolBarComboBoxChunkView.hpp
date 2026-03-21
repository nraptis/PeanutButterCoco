#import <Cocoa/Cocoa.h>

@interface ToolBarComboBoxChunkView : NSView

@property (strong, nonatomic, readonly) NSPopUpButton *comboBox;

- (instancetype)initWithItems:(NSArray<NSString *> *)items;

@end

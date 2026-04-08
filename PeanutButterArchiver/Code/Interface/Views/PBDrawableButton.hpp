#import <Cocoa/Cocoa.h>

@interface PBDrawableButton : NSControl

@property (copy, nonatomic) NSString *title;
@property (strong, nonatomic) NSColor *titleColor;
@property (strong, nonatomic) NSColor *disabledTitleColor;

@property (strong, nonatomic) NSColor *backgroundColor;
@property (strong, nonatomic) NSColor *pressedBackgroundColor;
@property (strong, nonatomic) NSColor *selectedBackgroundColor;
@property (strong, nonatomic) NSColor *selectedPressedBackgroundColor;

@property (strong, nonatomic) NSColor *borderColor;
@property (strong, nonatomic) NSColor *pressedBorderColor;
@property (strong, nonatomic) NSColor *selectedBorderColor;
@property (strong, nonatomic) NSColor *selectedPressedBorderColor;

@property (assign, nonatomic) CGFloat borderWidth;
@property (assign, nonatomic) CGFloat cornerRadius;
@property (assign, nonatomic) CGFloat preferredHeight;
@property (assign, nonatomic) NSEdgeInsets contentInsets;

@property (assign, nonatomic, getter=isSelected) BOOL selected;
@property (assign, nonatomic) BOOL togglesOnClick;

@end

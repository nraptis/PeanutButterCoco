#import <Cocoa/Cocoa.h>

@class ProgressPanelView;

@interface HomeActiveModeContainerView : NSView

@property (assign, nonatomic) BOOL showsProgressPanel;
@property (assign, nonatomic) NSInteger activeHomeTabIndex;
@property (strong, nonatomic, readonly) NSTextField *bundleSourceTextField;
@property (strong, nonatomic, readonly) NSTextField *bundleDestinationTextField;
@property (strong, nonatomic, readonly) NSTextField *bundleFilePrefixTextField;
@property (strong, nonatomic, readonly) NSButton *bundleSourceClearButton;
@property (strong, nonatomic, readonly) NSButton *bundleDestinationClearButton;
@property (strong, nonatomic, readonly) NSButton *bundleFilePrefixClearButton;
@property (strong, nonatomic, readonly) NSButton *bundleSourceBrowseFilesButton;
@property (strong, nonatomic, readonly) NSButton *bundleSourceBrowseButton;
@property (strong, nonatomic, readonly) NSButton *bundleDestinationBrowseButton;
@property (strong, nonatomic, readonly) NSButton *bundleRepairCheckbox;
@property (strong, nonatomic, readonly) NSButton *bundleIncludePreviewCheckbox;
@property (strong, nonatomic, readonly) NSPopUpButton *bundleRepairSizeCombo;
@property (strong, nonatomic, readonly) NSButton *bundleEncryptCheckbox;
@property (strong, nonatomic, readonly) NSPopUpButton *bundleEncryptionStrengthCombo;
@property (strong, nonatomic, readonly) NSPopUpButton *bundleTableStrengthCombo;
@property (strong, nonatomic, readonly) NSTextField *bundlePasswordTextField;
@property (strong, nonatomic, readonly) NSPopUpButton *bundleBlockCountCombo;
@property (strong, nonatomic, readonly) NSButton *bundleActionButton;
@property (strong, nonatomic, readonly) NSTextField *unbundleSourceTextField;
@property (strong, nonatomic, readonly) NSTextField *unbundleDestinationTextField;
@property (strong, nonatomic, readonly) NSTextField *unbundlePasswordTextField;
@property (strong, nonatomic, readonly) NSButton *unbundleSourceClearButton;
@property (strong, nonatomic, readonly) NSButton *unbundleDestinationClearButton;
@property (strong, nonatomic, readonly) NSButton *unbundleSourceBrowseFilesButton;
@property (strong, nonatomic, readonly) NSButton *unbundleSourceBrowseButton;
@property (strong, nonatomic, readonly) NSButton *unbundleDestinationBrowseButton;
@property (strong, nonatomic, readonly) NSButton *unbundleRecoverCheckbox;
@property (strong, nonatomic, readonly) NSButton *unbundleReadManifestButton;
@property (strong, nonatomic, readonly) NSButton *unbundleActionButton;
@property (strong, nonatomic, readonly) NSTextField *toolsSourceTextField;
@property (strong, nonatomic, readonly) NSTextField *toolsDestinationTextField;
@property (strong, nonatomic, readonly) NSButton *toolsSourceClearButton;
@property (strong, nonatomic, readonly) NSButton *toolsDestinationClearButton;
@property (strong, nonatomic, readonly) NSButton *toolsSourceBrowseButton;
@property (strong, nonatomic, readonly) NSButton *toolsDestinationBrowseButton;
@property (strong, nonatomic, readonly) NSButton *toolsIgnoreHiddenCheckbox;
@property (strong, nonatomic, readonly) NSButton *toolsActionButton;
@property (strong, nonatomic, readonly) NSButton *progressCancelButton;

+ (CGFloat)requiredHeight;
- (void)applyBundleDefaultsWithSource:(NSString *)source
                          destination:(NSString *)destination;
- (void)applyUnbundleDefaultsWithSource:(NSString *)source
                            destination:(NSString *)destination;
- (void)applyToolsDefaultsWithSource:(NSString *)source
                         destination:(NSString *)destination;
- (void)applyUnbundleRecoverDefaultEnabled:(BOOL)recoverEnabled;
- (void)setBundleSourcePathDropHandler:(void (^)(NSString *path))handler;
- (void)setBundleDestinationPathDropHandler:(void (^)(NSString *path))handler;
- (void)setUnbundleSourcePathDropHandler:(void (^)(NSString *path))handler;
- (void)setUnbundleDestinationPathDropHandler:(void (^)(NSString *path))handler;
- (void)setToolsSourcePathDropHandler:(void (^)(NSString *path))handler;
- (void)setToolsDestinationPathDropHandler:(void (^)(NSString *path))handler;
- (void)setShowsProgressPanel:(BOOL)showsProgressPanel;
- (void)setBundleControlsEnabled:(BOOL)enabled;
- (void)updateProgressTitle:(NSString *)title
                     detail:(NSString *)detail
                   fraction:(double)fraction;

@end

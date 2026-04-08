#import <Cocoa/Cocoa.h>

#import "../Views/HomeHeaderView.hpp"

@class AppShell;
@class AppConfigStore;
@class HomeContainerViewController;

@interface RootViewController : NSViewController <NSTextFieldDelegate>

@property (strong, nonatomic) HomeContainerViewController *homeContainerViewController;
@property (strong, nonatomic) AppConfigStore *configStore;
@property (copy, nonatomic) NSString *bundleSourceDefault;
@property (copy, nonatomic) NSString *bundleDestinationDefault;
@property (copy, nonatomic) NSString *unbundleSourceDefault;
@property (copy, nonatomic) NSString *unbundleDestinationDefault;
@property (copy, nonatomic) NSString *toolsSourceDefault;
@property (copy, nonatomic) NSString *toolsDestinationDefault;
@property (copy, nonatomic) NSString *bundleFilePrefixDefault;
@property (copy, nonatomic) NSString *bundlePasswordDefault;
@property (copy, nonatomic) NSString *unbundlePasswordDefault;
@property (assign, nonatomic) BOOL unbundleRecoverDefault;
@property (copy, nonatomic) NSString *bundleBlockCountDefault;
@property (copy, nonatomic) NSString *bundleEncryptionStrengthDefault;
@property (copy, nonatomic) NSString *bundleTableStrengthDefault;
@property (copy, nonatomic) NSString *bundleRepairSizeDefault;
@property (assign, nonatomic) BOOL bundleRepairDefault;
@property (assign, nonatomic) BOOL bundleSafeDefault;
@property (assign, nonatomic) BOOL bundleEncryptDefault;
@property (assign, nonatomic) BOOL bundleIncludePreviewDefault;
@property (assign, nonatomic) NSInteger homeTabDefault;

@end

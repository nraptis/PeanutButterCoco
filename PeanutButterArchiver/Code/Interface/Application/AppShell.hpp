#import <Cocoa/Cocoa.h>

@class HomeContainerViewController;

@interface AppShell : NSObject

- (instancetype)initWithHomeContainerViewController:(HomeContainerViewController *)homeContainerViewController;
- (void)startPolling;
- (void)stopPolling;
- (void)enqueueBundleRequestWithSourceDirectory:(NSString *)sourceDirectory
                           destinationDirectory:(NSString *)destinationDirectory
                                     filePrefix:(NSString *)filePrefix
                                repairEnabled:(BOOL)repairEnabled
                                  safeEnabled:(BOOL)safeEnabled
                            encryptionEnabled:(BOOL)encryptionEnabled
                          includePreviewEnabled:(BOOL)includePreviewEnabled
                                       password:(NSString *)password
                                 blockCountTitle:(NSString *)blockCountTitle
                        encryptionStrengthTitle:(NSString *)encryptionStrengthTitle
                              tableStrengthTitle:(NSString *)tableStrengthTitle;
- (void)enqueueUnbundleRequestWithSourcePath:(NSString *)sourcePath
                        destinationDirectory:(NSString *)destinationDirectory
                               recoverEnabled:(BOOL)recoverEnabled
                                    password:(NSString *)password;
- (void)enqueueManifestRequestWithSourcePath:(NSString *)sourcePath
                        destinationDirectory:(NSString *)destinationDirectory
                                    password:(NSString *)password;
- (void)enqueueRepairRequestWithSourcePath:(NSString *)sourcePath
                      destinationDirectory:(NSString *)destinationDirectory
                                  password:(NSString *)password;
- (void)enqueueSanityRequestWithLeftDirectory:(NSString *)leftDirectory
                               rightDirectory:(NSString *)rightDirectory
                                 ignoreHidden:(BOOL)ignoreHidden;
- (void)enqueueCancelRequest;

@end

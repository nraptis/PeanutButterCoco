#pragma once

#import <Foundation/Foundation.h>

#ifdef __cplusplus
#include "../../Common/AppConfigState.hpp"
#endif

@interface AppConfigStore : NSObject

- (NSURL * _Nonnull)configDirectoryURL;
- (NSURL * _Nonnull)configFileURL;
#ifdef __cplusplus
- (peanutbutter::AppConfigStateV2)loadOrCreateConfig;
- (BOOL)saveConfig:(const peanutbutter::AppConfigStateV2&)configState
             error:(NSError * _Nullable * _Nullable)error;
#endif
- (BOOL)saveBundleUiStateWithHomeTab:(NSInteger)homeTab
                             source:(NSString * _Nullable)source
                        destination:(NSString * _Nullable)destination
                         filePrefix:(NSString * _Nullable)filePrefix
                       repairEnabled:(BOOL)repairEnabled
                         safeEnabled:(BOOL)safeEnabled
                   encryptionEnabled:(BOOL)encryptionEnabled
               includePreviewEnabled:(BOOL)includePreviewEnabled
                      blockCountTitle:(NSString * _Nullable)blockCountTitle
             encryptionStrengthTitle:(NSString * _Nullable)encryptionStrengthTitle
                   tableStrengthTitle:(NSString * _Nullable)tableStrengthTitle
                    repairSizeTitle:(NSString * _Nullable)repairSizeTitle
                            password:(NSString * _Nullable)password
                               error:(NSError * _Nullable * _Nullable)error;
- (BOOL)saveUnbundleUiStateWithHomeTab:(NSInteger)homeTab
                                source:(NSString * _Nullable)source
                           destination:(NSString * _Nullable)destination
                         recoverEnabled:(BOOL)recoverEnabled
                              password:(NSString * _Nullable)password
                                 error:(NSError * _Nullable * _Nullable)error;

@end

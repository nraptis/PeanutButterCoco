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
- (BOOL)saveHomeUiStateWithHomeTab:(NSInteger)homeTab
                         inputText:(NSString * _Nullable)inputText
                      archivedText:(NSString * _Nullable)archivedText
                    unarchivedText:(NSString * _Nullable)unarchivedText
                      compareAText:(NSString * _Nullable)compareAText
                      compareBText:(NSString * _Nullable)compareBText
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
                  recoverEnabled:(BOOL)recoverEnabled
               unbundlePassword:(NSString * _Nullable)unbundlePassword
                            error:(NSError * _Nullable * _Nullable)error;

@end

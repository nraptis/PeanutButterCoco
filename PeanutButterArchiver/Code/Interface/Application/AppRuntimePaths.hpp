#pragma once

#import <Foundation/Foundation.h>

@interface AppRuntimePaths : NSObject

+ (NSURL * _Nonnull)appBundleURL;
+ (NSURL * _Nonnull)appContainerDirectoryURL;
+ (NSURL * _Nonnull)applicationSupportDirectoryURL;
+ (NSArray<NSURL *> * _Nonnull)activeSearchRootDirectoryURLs;
+ (NSURL * _Nonnull)activeSearchRootDirectoryURL;
+ (BOOL)shouldUseAppContainerAsWorkingDirectory;
+ (BOOL)applyAppContainerAsCurrentWorkingDirectory;

@end

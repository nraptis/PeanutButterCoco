#import "AppRuntimePaths.hpp"

namespace {

NSString * const kApplicationSupportDirectoryName = @"PeanutButterArchiver";

NSURL *FallbackCurrentDirectoryURL(void) {
    NSString *path = [[NSFileManager defaultManager] currentDirectoryPath] ?: @".";
    return [NSURL fileURLWithPath:path isDirectory:YES];
}

NSURL *StandardizedDirectoryURL(NSString *path) {
    if (path.length == 0) {
        return nil;
    }
    return [[NSURL fileURLWithPath:path isDirectory:YES] URLByStandardizingPath];
}

void AddUniqueDirectoryURL(NSMutableArray<NSURL *> *results,
                           NSMutableSet<NSString *> *seenPaths,
                           NSURL *url) {
    NSString *path = url.path ?: @"";
    if (path.length == 0 || [seenPaths containsObject:path]) {
        return;
    }
    [seenPaths addObject:path];
    [results addObject:url];
}

}  // namespace

@implementation AppRuntimePaths

+ (NSURL *)appBundleURL {
    NSURL *bundleURL = [NSBundle mainBundle].bundleURL;
    if (bundleURL == nil) {
        return FallbackCurrentDirectoryURL();
    }
    return bundleURL;
}

+ (NSURL *)appContainerDirectoryURL {
    NSURL *bundleURL = [self appBundleURL];
    NSURL *containerURL = [bundleURL URLByDeletingLastPathComponent];
    if (containerURL == nil) {
        return FallbackCurrentDirectoryURL();
    }
    return containerURL;
}

+ (NSURL *)applicationSupportDirectoryURL {
    NSURL *baseURL = [[[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory
                                                             inDomains:NSUserDomainMask] firstObject];
    if (baseURL == nil) {
        return [[self appContainerDirectoryURL]
            URLByAppendingPathComponent:kApplicationSupportDirectoryName
                            isDirectory:YES];
    }
    return [baseURL URLByAppendingPathComponent:kApplicationSupportDirectoryName
                                    isDirectory:YES];
}

+ (NSArray<NSURL *> *)activeSearchRootDirectoryURLs {
#if PB_APP_USE_BUNDLE_PARENT_ROOT
    return @[[self appContainerDirectoryURL]];
#else
    NSMutableArray<NSURL *> *results = [NSMutableArray array];
    NSMutableSet<NSString *> *seenPaths = [NSMutableSet set];
    NSDictionary<NSString *, NSString *> *environment = [NSProcessInfo processInfo].environment;

    NSString *overridePath = environment[@"PB_APP_DEBUG_ROOT"];
    AddUniqueDirectoryURL(results, seenPaths, StandardizedDirectoryURL(overridePath));

    NSString *pwdPath = environment[@"PWD"];
    AddUniqueDirectoryURL(results, seenPaths, StandardizedDirectoryURL(pwdPath));

    NSString *projectDir = environment[@"PROJECT_DIR"];
    if (projectDir.length > 0) {
        AddUniqueDirectoryURL(results,
                              seenPaths,
                              StandardizedDirectoryURL([projectDir stringByAppendingPathComponent:@".."]));
    }

    NSString *sourceRoot = environment[@"SRCROOT"];
    if (sourceRoot.length > 0) {
        AddUniqueDirectoryURL(results,
                              seenPaths,
                              StandardizedDirectoryURL([sourceRoot stringByAppendingPathComponent:@".."]));
        AddUniqueDirectoryURL(results, seenPaths, StandardizedDirectoryURL(sourceRoot));
    }

    AddUniqueDirectoryURL(results, seenPaths, FallbackCurrentDirectoryURL());

    if (results.count == 0) {
        AddUniqueDirectoryURL(results, seenPaths, [self appContainerDirectoryURL]);
    }
    return results;
#endif
}

+ (NSURL *)activeSearchRootDirectoryURL {
    return [self activeSearchRootDirectoryURLs].firstObject ?: FallbackCurrentDirectoryURL();
}

+ (BOOL)shouldUseAppContainerAsWorkingDirectory {
#if PB_APP_USE_BUNDLE_PARENT_ROOT
    return YES;
#else
    return NO;
#endif
}

+ (BOOL)applyAppContainerAsCurrentWorkingDirectory {
    NSString *path = [self appContainerDirectoryURL].path;
    if (path.length == 0) {
        return NO;
    }
    return [[NSFileManager defaultManager] changeCurrentDirectoryPath:path];
}

@end

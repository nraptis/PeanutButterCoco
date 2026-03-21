#import "AppConfigStore.hpp"
#import "AppRuntimePaths.hpp"

namespace {

using peanutbutter::AppConfigStateV2;
using peanutbutter::HomeTabV2;

NSString * const kConfigFileName = @"config.json";

NSString *StringFromStd(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()] ?: @"";
}

std::string StdFromString(NSString *value) {
    if (value == nil) {
        return std::string();
    }
    return std::string(value.UTF8String ?: "");
}

NSInteger IntegerFromHomeTab(HomeTabV2 tab) {
    switch (tab) {
        case HomeTabV2::kBundle:
            return 0;
        case HomeTabV2::kUnbundle:
            return 1;
        case HomeTabV2::kRepair:
            return 2;
        case HomeTabV2::kSanity:
            return 3;
    }
    return 0;
}

HomeTabV2 HomeTabFromInteger(NSInteger value) {
    switch (value) {
        case 1:
            return HomeTabV2::kUnbundle;
        case 2:
            return HomeTabV2::kRepair;
        case 3:
            return HomeTabV2::kSanity;
        default:
            return HomeTabV2::kBundle;
    }
}

NSDictionary *DictionaryFromConfig(const AppConfigStateV2& configState) {
    return @{
        @"home_tab": @(IntegerFromHomeTab(configState.mHomeTab)),

        @"bundle_directory_source": StringFromStd(configState.mBundleDirectorySource),
        @"bundle_directory_destination": StringFromStd(configState.mBundleDirectoryDestination),
        @"bundle_file_prefix": StringFromStd(configState.mBundleFilePrefix),
        @"bundle_repair": @(configState.mBundleRepair),
        @"bundle_safe": @(configState.mBundleSafe),
        @"bundle_encrypt": @(configState.mBundleEncrypt),
        @"bundle_include_preview": @(configState.mBundleIncludePreview),
        @"bundle_block_count": StringFromStd(configState.mBundleBlockCount),
        @"bundle_encryption_strength": StringFromStd(configState.mBundleEncryptionStrength),
        @"bundle_table_strength": StringFromStd(configState.mBundleTableStrength),
        @"bundle_repair_size": StringFromStd(configState.mBundleRepairSize),
        @"bundle_password": StringFromStd(configState.mBundlePassword),

        @"unbundle_directory_source": StringFromStd(configState.mUnbundleDirectorySource),
        @"unbundle_directory_destination": StringFromStd(configState.mUnbundleDirectoryDestination),
        @"unbundle_encrypt": @(configState.mUnbundleEncrypt),
        @"unbundle_password": StringFromStd(configState.mUnbundlePassword),

        @"repair_directory_source": StringFromStd(configState.mRepairDirectorySource),
        @"repair_directory_destination": StringFromStd(configState.mRepairDirectoryDestination),
        @"repair_encrypt": @(configState.mRepairEncrypt),
        @"repair_password": StringFromStd(configState.mRepairPassword),

        @"sanity_directory_source": StringFromStd(configState.mSanityDirectorySource),
        @"sanity_directory_destination": StringFromStd(configState.mSanityDirectoryDestination),
    };
}

AppConfigStateV2 ConfigFromDictionary(NSDictionary *dictionary) {
    AppConfigStateV2 configState;
    if (dictionary == nil) {
        return configState;
    }

    configState.mHomeTab = HomeTabFromInteger([dictionary[@"home_tab"] integerValue]);

    configState.mBundleDirectorySource = StdFromString(dictionary[@"bundle_directory_source"]);
    configState.mBundleDirectoryDestination = StdFromString(dictionary[@"bundle_directory_destination"]);
    configState.mBundleFilePrefix = StdFromString(dictionary[@"bundle_file_prefix"]);
    configState.mBundleRepair = [dictionary[@"bundle_repair"] boolValue];
    configState.mBundleSafe = [dictionary[@"bundle_safe"] boolValue];
    configState.mBundleEncrypt = [dictionary[@"bundle_encrypt"] boolValue];
    configState.mBundleIncludePreview =
        dictionary[@"bundle_include_preview"] == nil ? YES : [dictionary[@"bundle_include_preview"] boolValue];
    configState.mBundleBlockCount = StdFromString(dictionary[@"bundle_block_count"]);
    configState.mBundleEncryptionStrength = StdFromString(dictionary[@"bundle_encryption_strength"]);
    configState.mBundleTableStrength = StdFromString(dictionary[@"bundle_table_strength"]);
    configState.mBundleRepairSize = StdFromString(dictionary[@"bundle_repair_size"]);
    configState.mBundlePassword = StdFromString(dictionary[@"bundle_password"]);

    configState.mUnbundleDirectorySource = StdFromString(dictionary[@"unbundle_directory_source"]);
    configState.mUnbundleDirectoryDestination = StdFromString(dictionary[@"unbundle_directory_destination"]);
    configState.mUnbundleEncrypt = [dictionary[@"unbundle_encrypt"] boolValue];
    configState.mUnbundlePassword = StdFromString(dictionary[@"unbundle_password"]);

    configState.mRepairDirectorySource = StdFromString(dictionary[@"repair_directory_source"]);
    configState.mRepairDirectoryDestination = StdFromString(dictionary[@"repair_directory_destination"]);
    configState.mRepairEncrypt = [dictionary[@"repair_encrypt"] boolValue];
    configState.mRepairPassword = StdFromString(dictionary[@"repair_password"]);

    configState.mSanityDirectorySource = StdFromString(dictionary[@"sanity_directory_source"]);
    configState.mSanityDirectoryDestination = StdFromString(dictionary[@"sanity_directory_destination"]);

    return configState;
}

}  // namespace

@implementation AppConfigStore

- (NSURL *)configDirectoryURL {
    return [AppRuntimePaths applicationSupportDirectoryURL];
}

- (NSURL *)configFileURL {
    return [[self configDirectoryURL] URLByAppendingPathComponent:kConfigFileName isDirectory:NO];
}

- (peanutbutter::AppConfigStateV2)loadOrCreateConfig {
    NSURL *configURL = [self configFileURL];
    NSData *data = [NSData dataWithContentsOfURL:configURL];
    if (data == nil) {
        AppConfigStateV2 configState;
        [self saveConfig:configState error:nil];
        return configState;
    }

    id jsonObject = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
    if (![jsonObject isKindOfClass:[NSDictionary class]]) {
        AppConfigStateV2 configState;
        [self saveConfig:configState error:nil];
        return configState;
    }

    return ConfigFromDictionary((NSDictionary *)jsonObject);
}

- (BOOL)saveConfig:(const peanutbutter::AppConfigStateV2&)configState
             error:(NSError * _Nullable * _Nullable)error {
    NSURL *directoryURL = [self configDirectoryURL];
    if (![[NSFileManager defaultManager] createDirectoryAtURL:directoryURL
                                  withIntermediateDirectories:YES
                                                   attributes:nil
                                                        error:error]) {
        return NO;
    }

    NSDictionary *dictionary = DictionaryFromConfig(configState);
    NSData *data = [NSJSONSerialization dataWithJSONObject:dictionary
                                                   options:NSJSONWritingPrettyPrinted
                                                     error:error];
    if (data == nil) {
        return NO;
    }

    return [data writeToURL:[self configFileURL] options:NSDataWritingAtomic error:error];
}

- (BOOL)saveBundleUiStateWithHomeTab:(NSInteger)homeTab
                             source:(NSString *)source
                        destination:(NSString *)destination
                         filePrefix:(NSString *)filePrefix
                       repairEnabled:(BOOL)repairEnabled
                         safeEnabled:(BOOL)safeEnabled
                   encryptionEnabled:(BOOL)encryptionEnabled
               includePreviewEnabled:(BOOL)includePreviewEnabled
                      blockCountTitle:(NSString *)blockCountTitle
             encryptionStrengthTitle:(NSString *)encryptionStrengthTitle
                   tableStrengthTitle:(NSString *)tableStrengthTitle
                    repairSizeTitle:(NSString *)repairSizeTitle
                            password:(NSString *)password
                               error:(NSError * _Nullable * _Nullable)error {
    AppConfigStateV2 configState = [self loadOrCreateConfig];
    configState.mHomeTab = HomeTabFromInteger(homeTab);
    configState.mBundleDirectorySource = StdFromString(source);
    configState.mBundleDirectoryDestination = StdFromString(destination);
    configState.mBundleFilePrefix = StdFromString(filePrefix);
    configState.mBundleRepair = repairEnabled;
    configState.mBundleSafe = safeEnabled;
    configState.mBundleEncrypt = encryptionEnabled;
    configState.mBundleIncludePreview = includePreviewEnabled;
    configState.mBundleBlockCount = StdFromString(blockCountTitle);
    configState.mBundleEncryptionStrength = StdFromString(encryptionStrengthTitle);
    configState.mBundleTableStrength = StdFromString(tableStrengthTitle);
    configState.mBundleRepairSize = StdFromString(repairSizeTitle);
    configState.mBundlePassword = StdFromString(password);
    return [self saveConfig:configState error:error];
}

- (BOOL)saveUnbundleUiStateWithHomeTab:(NSInteger)homeTab
                                source:(NSString *)source
                           destination:(NSString *)destination
                              password:(NSString *)password
                                 error:(NSError * _Nullable * _Nullable)error {
    AppConfigStateV2 configState = [self loadOrCreateConfig];
    configState.mHomeTab = HomeTabFromInteger(homeTab);
    configState.mUnbundleDirectorySource = StdFromString(source);
    configState.mUnbundleDirectoryDestination = StdFromString(destination);
    configState.mUnbundlePassword = StdFromString(password);
    return [self saveConfig:configState error:error];
}

@end

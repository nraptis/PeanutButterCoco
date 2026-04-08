#import "AppConfigStore.hpp"
#import "AppRuntimePaths.hpp"

namespace {

using peanutbutter::AppConfigStateV2;
using peanutbutter::HomeTabV2;

NSString * const kConfigFileName = @"config.json";

NSString *StringFromStd(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()] ?: @"";
}

#if PB_APP_USE_BUNDLE_PARENT_ROOT
NSString *PortableConfigFileName(void) {
    NSString *bundleName =
        [[[[AppRuntimePaths appBundleURL] lastPathComponent] stringByDeletingPathExtension] copy];
    if (bundleName.length == 0) {
        bundleName = @"PBCrypt";
    }
    return [bundleName stringByAppendingString:@".config.json"];
}
#endif

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
        case HomeTabV2::kTools:
            return 2;
    }
    return 0;
}

HomeTabV2 HomeTabFromInteger(NSInteger value) {
    switch (value) {
        case 1:
            return HomeTabV2::kUnbundle;
        case 2:
        case 3:
            return HomeTabV2::kTools;
        default:
            return HomeTabV2::kBundle;
    }
}

BOOL DictionaryNeedsRewrite(NSDictionary *dictionary) {
    if (dictionary == nil) {
        return NO;
    }

    if (dictionary[@"text_field_input_text"] == nil ||
        dictionary[@"text_field_archived_text"] == nil ||
        dictionary[@"text_field_unarchived_text"] == nil ||
        dictionary[@"text_field_compare_a_text"] == nil ||
        dictionary[@"text_field_compare_b_text"] == nil ||
        dictionary[@"window_frame_x"] == nil ||
        dictionary[@"window_frame_y"] == nil ||
        dictionary[@"window_frame_width"] == nil ||
        dictionary[@"window_frame_height"] == nil) {
        return YES;
    }

    if (dictionary[@"bundle_directory_source"] != nil ||
        dictionary[@"bundle_directory_destination"] != nil ||
        dictionary[@"unbundle_directory_source"] != nil ||
        dictionary[@"unbundle_directory_destination"] != nil ||
        dictionary[@"repair_directory_source"] != nil ||
        dictionary[@"repair_directory_destination"] != nil ||
        dictionary[@"sanity_directory_source"] != nil ||
        dictionary[@"sanity_directory_destination"] != nil) {
        return YES;
    }

    NSInteger homeTab = [dictionary[@"home_tab"] integerValue];
    return (homeTab < 0 || homeTab > 2);
}

NSDictionary *DictionaryFromConfig(const AppConfigStateV2& configState) {
    return @{
        @"home_tab": @(IntegerFromHomeTab(configState.mHomeTab)),
        @"window_frame_x": @(configState.mWindowFrameX),
        @"window_frame_y": @(configState.mWindowFrameY),
        @"window_frame_width": @(configState.mWindowFrameWidth),
        @"window_frame_height": @(configState.mWindowFrameHeight),

        @"text_field_input_text": StringFromStd(configState.mTextFieldInputText),
        @"text_field_archived_text": StringFromStd(configState.mTextFieldArchivedText),
        @"text_field_unarchived_text": StringFromStd(configState.mTextFieldUnarchivedText),
        @"text_field_compare_a_text": StringFromStd(configState.mTextFieldCompareAText),
        @"text_field_compare_b_text": StringFromStd(configState.mTextFieldCompareBText),

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

        @"unbundle_encrypt": @(configState.mUnbundleEncrypt),
        @"unbundle_recover": @(configState.mUnbundleRecover),
        @"unbundle_password": StringFromStd(configState.mUnbundlePassword),
    };
}

AppConfigStateV2 ConfigFromDictionary(NSDictionary *dictionary) {
    AppConfigStateV2 configState;
    if (dictionary == nil) {
        return configState;
    }

    configState.mHomeTab = HomeTabFromInteger([dictionary[@"home_tab"] integerValue]);
    configState.mWindowFrameX = [dictionary[@"window_frame_x"] doubleValue];
    configState.mWindowFrameY = [dictionary[@"window_frame_y"] doubleValue];
    configState.mWindowFrameWidth = [dictionary[@"window_frame_width"] doubleValue];
    configState.mWindowFrameHeight = [dictionary[@"window_frame_height"] doubleValue];

    configState.mTextFieldInputText = StdFromString(dictionary[@"text_field_input_text"]);
    if (configState.mTextFieldInputText.empty()) {
        configState.mTextFieldInputText = StdFromString(dictionary[@"bundle_directory_source"]);
    }

    configState.mTextFieldArchivedText = StdFromString(dictionary[@"text_field_archived_text"]);
    if (configState.mTextFieldArchivedText.empty()) {
        configState.mTextFieldArchivedText =
            StdFromString(dictionary[@"unbundle_directory_source"]);
    }
    if (configState.mTextFieldArchivedText.empty()) {
        configState.mTextFieldArchivedText =
            StdFromString(dictionary[@"bundle_directory_destination"]);
    }

    configState.mTextFieldUnarchivedText =
        StdFromString(dictionary[@"text_field_unarchived_text"]);
    if (configState.mTextFieldUnarchivedText.empty()) {
        configState.mTextFieldUnarchivedText =
            StdFromString(dictionary[@"unbundle_directory_destination"]);
    }

    configState.mTextFieldCompareAText =
        StdFromString(dictionary[@"text_field_compare_a_text"]);
    if (configState.mTextFieldCompareAText.empty()) {
        configState.mTextFieldCompareAText =
            StdFromString(dictionary[@"sanity_directory_source"]);
    }
    if (configState.mTextFieldCompareAText.empty()) {
        configState.mTextFieldCompareAText = configState.mTextFieldInputText;
    }

    configState.mTextFieldCompareBText =
        StdFromString(dictionary[@"text_field_compare_b_text"]);
    if (configState.mTextFieldCompareBText.empty()) {
        configState.mTextFieldCompareBText =
            StdFromString(dictionary[@"sanity_directory_destination"]);
    }
    if (configState.mTextFieldCompareBText.empty()) {
        configState.mTextFieldCompareBText = configState.mTextFieldUnarchivedText;
    }

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

    configState.mUnbundleEncrypt =
        dictionary[@"unbundle_encrypt"] == nil ? YES : [dictionary[@"unbundle_encrypt"] boolValue];
    configState.mUnbundleRecover =
        dictionary[@"unbundle_recover"] == nil ? NO : [dictionary[@"unbundle_recover"] boolValue];
    configState.mUnbundlePassword = StdFromString(dictionary[@"unbundle_password"]);

    return configState;
}

}  // namespace

@implementation AppConfigStore

- (NSURL *)configDirectoryURL {
#if PB_APP_USE_BUNDLE_PARENT_ROOT
    return [AppRuntimePaths appContainerDirectoryURL];
#else
    return [AppRuntimePaths applicationSupportDirectoryURL];
#endif
}

- (NSURL *)configFileURL {
#if PB_APP_USE_BUNDLE_PARENT_ROOT
    return [[self configDirectoryURL] URLByAppendingPathComponent:PortableConfigFileName()
                                                      isDirectory:NO];
#else
    return [[self configDirectoryURL] URLByAppendingPathComponent:kConfigFileName isDirectory:NO];
#endif
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

    NSDictionary *dictionary = (NSDictionary *)jsonObject;
    AppConfigStateV2 configState = ConfigFromDictionary(dictionary);
    if (DictionaryNeedsRewrite(dictionary)) {
        [self saveConfig:configState error:nil];
    }
    return configState;
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

- (BOOL)saveHomeUiStateWithHomeTab:(NSInteger)homeTab
                         inputText:(NSString *)inputText
                      archivedText:(NSString *)archivedText
                    unarchivedText:(NSString *)unarchivedText
                      compareAText:(NSString *)compareAText
                      compareBText:(NSString *)compareBText
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
                  recoverEnabled:(BOOL)recoverEnabled
               unbundlePassword:(NSString *)unbundlePassword
                            error:(NSError * _Nullable * _Nullable)error {
    AppConfigStateV2 configState = [self loadOrCreateConfig];
    configState.mHomeTab = HomeTabFromInteger(homeTab);
    configState.mTextFieldInputText = StdFromString(inputText);
    configState.mTextFieldArchivedText = StdFromString(archivedText);
    configState.mTextFieldUnarchivedText = StdFromString(unarchivedText);
    configState.mTextFieldCompareAText = StdFromString(compareAText);
    configState.mTextFieldCompareBText = StdFromString(compareBText);
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
    configState.mUnbundleRecover = recoverEnabled;
    configState.mUnbundlePassword = StdFromString(unbundlePassword);
    return [self saveConfig:configState error:error];
}

@end

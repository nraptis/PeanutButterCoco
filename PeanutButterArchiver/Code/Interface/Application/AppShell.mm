#import "AppShell.hpp"

#include <atomic>
#include <memory>
#include <vector>

#include "../../Knobs.hpp"
#import "../../Common/BundleRequest.hpp"
#import "../../Common/DecodeRequest.hpp"
#import "../../Common/LogCatalog.hpp"
#import "../../Common/RepairRequest.hpp"
#import "../../Common/SanityRequest.hpp"
#import "../../Engine/ArchiverExecutor.hpp"
#import "AppRuntimePaths.hpp"
#import "../ViewControllers/HomeContainerViewController.hpp"
#import "../Views/HomeActiveModeContainerView.hpp"
#import "../Views/HomeHeaderView.hpp"
#import "../Views/HomeLogContainerView.hpp"
#import "../Views/HomeLogView.hpp"
#import "../Views/HomeToolViewSplitter.hpp"
#import "../Views/HomeToolViewTop.hpp"

static NSString *PBNSStringFromStdString(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()] ?: @"";
}

static std::string PBStdStringFromNSString(NSString *value) {
    if (value == nil) {
        return std::string();
    }

    NSData *data = [value dataUsingEncoding:NSUTF8StringEncoding allowLossyConversion:YES];
    if (data == nil || data.length == 0) {
        return std::string();
    }

    return std::string(static_cast<const char *>(data.bytes), static_cast<std::size_t>(data.length));
}

static void PBAppendRuntimeInfoPart(const peanutbutter::RuntimeEventV2& event,
                                    const char *key,
                                    const char *prefix,
                                    std::vector<std::string>& parts) {
    const std::string value = event.FetchInfo(key);
    if (!value.empty()) {
        parts.push_back(std::string(prefix) + value);
    }
}

static bool PBShouldRenderRuntimeLog(const peanutbutter::RuntimeEventV2& event) {
    return event.mKind != peanutbutter::RuntimeEventKindV2::kBundleFileFinished &&
           event.mKind != peanutbutter::RuntimeEventKindV2::kDecodeFileFinished;
}

static std::string PBCompactRuntimeEventLine(const peanutbutter::RuntimeEventV2& event) {
    std::string line = "[Runtime][" +
        peanutbutter::ProgressStageLabelV2(event.mStage) + "][" +
        peanutbutter::RuntimeEventKindLabelV2(event.mKind) + "]";

    std::vector<std::string> parts;
    parts.reserve(8);
    PBAppendRuntimeInfoPart(event, "archive_index", "a=", parts);
    PBAppendRuntimeInfoPart(event, "block_index", "b=", parts);
    PBAppendRuntimeInfoPart(event, "family_block_index", "fb=", parts);
    PBAppendRuntimeInfoPart(event, "destination_archive_index", "da=", parts);
    PBAppendRuntimeInfoPart(event, "destination_block_index", "db=", parts);
    PBAppendRuntimeInfoPart(event, "source_archive_index", "sa=", parts);
    PBAppendRuntimeInfoPart(event, "source_block_index", "sb=", parts);
    PBAppendRuntimeInfoPart(event, "section_type", "sec=", parts);

    for (const std::string& part : parts) {
        line += " ";
        line += part;
    }
    return line;
}

static std::string PBCompactCheckpointLine(const peanutbutter::EngineCheckpointRequestV2& checkpoint) {
    return "[Checkpoint #" + std::to_string(checkpoint.mCheckpointId) + "] " +
           PBCompactRuntimeEventLine(checkpoint.mRuntimeEvent);
}

typedef NS_ENUM(NSInteger, PBResolvedPathMode) {
    PBResolvedPathModeSource = 0,
    PBResolvedPathModeDestination = 1,
};

static NSString *PBTrimmedString(NSString *value) {
    NSString *safeValue = [[value ?: @"" stringByTrimmingCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]] copy];
    if (safeValue.length == 0) {
        return @"";
    }
    return safeValue;
}

static BOOL PBURLExists(NSURL *url, BOOL *isDirectory) {
    if (url == nil) {
        return NO;
    }

    NSNumber *isDirectoryValue = nil;
    BOOL reachable = [url checkResourceIsReachableAndReturnError:nil];
    if (reachable) {
        [url getResourceValue:&isDirectoryValue forKey:NSURLIsDirectoryKey error:nil];
    }
    if (isDirectory != NULL) {
        *isDirectory = reachable && isDirectoryValue.boolValue;
    }
    return reachable;
}

static NSURL *PBStandardizedFileURL(NSString *path) {
    if (path.length == 0) {
        return nil;
    }
    return [[NSURL fileURLWithPath:path isDirectory:NO] URLByStandardizingPath];
}

static NSString *PBLastPathComponentFromInput(NSString *input) {
    NSString *lastComponent = input.lastPathComponent;
    return lastComponent.length > 0 ? lastComponent : input;
}

static BOOL PBInputLooksDirectoryLike(NSString *input) {
    return [PBLastPathComponentFromInput(input) pathExtension].length == 0;
}

static NSURL *PBFindDescendantNamed(NSURL *rootURL,
                                    NSString *name,
                                    BOOL directoriesOnly) {
    if (rootURL == nil || name.length == 0) {
        return nil;
    }

    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSDirectoryEnumerator<NSURL *> *enumerator =
        [fileManager enumeratorAtURL:rootURL
          includingPropertiesForKeys:@[NSURLNameKey, NSURLIsDirectoryKey]
                             options:(NSDirectoryEnumerationSkipsHiddenFiles |
                                      NSDirectoryEnumerationSkipsPackageDescendants)
                        errorHandler:nil];
    if (enumerator == nil) {
        return nil;
    }

    NSMutableArray<NSURL *> *matches = [NSMutableArray array];
    for (NSURL *candidateURL in enumerator) {
        NSString *candidateName = nil;
        NSNumber *isDirectoryValue = nil;
        [candidateURL getResourceValue:&candidateName forKey:NSURLNameKey error:nil];
        [candidateURL getResourceValue:&isDirectoryValue forKey:NSURLIsDirectoryKey error:nil];
        if (![candidateName isEqualToString:name]) {
            continue;
        }
        if (directoriesOnly && !isDirectoryValue.boolValue) {
            continue;
        }
        [matches addObject:candidateURL];
    }

    if (matches.count == 0) {
        return nil;
    }

    [matches sortUsingComparator:^NSComparisonResult(NSURL *left, NSURL *right) {
        return [left.path compare:right.path options:NSCaseInsensitiveSearch];
    }];
    return matches.firstObject;
}

static NSString *PBResolvedDestinationFallbackPath(NSURL *candidateURL, NSURL *rootURL) {
    NSString *candidatePath = candidateURL.path ?: @"";
    NSString *parentPath = candidatePath.stringByDeletingLastPathComponent;
    if (parentPath.length == 0) {
        return rootURL.path ?: @"";
    }
    return parentPath;
}

static peanutbutter::RepairCoveragePresetV2 PBRepairCoverageFromTitle(NSString *title) {
    NSString *safeTitle = PBTrimmedString(title);
    if ([safeTitle isEqualToString:@"40%"]) {
        return peanutbutter::RepairCoveragePresetV2::k40;
    }
    if ([safeTitle isEqualToString:@"60%"]) {
        return peanutbutter::RepairCoveragePresetV2::k60;
    }
    if ([safeTitle isEqualToString:@"80%"]) {
        return peanutbutter::RepairCoveragePresetV2::k80;
    }
    return peanutbutter::RepairCoveragePresetV2::k20;
}

static NSString *PBResolvePathString(NSString *value, PBResolvedPathMode mode) {
    NSString *safeValue = PBTrimmedString(value);
    if (safeValue.length == 0) {
        return @"";
    }

    NSString *expanded = [safeValue stringByExpandingTildeInPath];
    NSArray<NSURL *> *rootURLs = [AppRuntimePaths activeSearchRootDirectoryURLs];
    NSURL *rootURL = rootURLs.firstObject ?: [AppRuntimePaths activeSearchRootDirectoryURL];
    BOOL sourceWantsDirectory = (mode == PBResolvedPathModeSource) && PBInputLooksDirectoryLike(expanded);
    BOOL destinationLooksDirectory = (mode == PBResolvedPathModeDestination) && PBInputLooksDirectoryLike(expanded);
    NSURL *candidateURL = nil;
    if ([expanded isAbsolutePath]) {
        candidateURL = PBStandardizedFileURL(expanded);
    } else {
        for (NSURL *candidateRootURL in rootURLs) {
            NSString *rootPath = candidateRootURL.path ?: @"";
            NSURL *rootCandidateURL =
                PBStandardizedFileURL([rootPath stringByAppendingPathComponent:expanded]);
            BOOL rootCandidateIsDirectory = NO;
            if (!PBURLExists(rootCandidateURL, &rootCandidateIsDirectory)) {
                continue;
            }
            if (sourceWantsDirectory && !rootCandidateIsDirectory) {
                continue;
            }
            if (destinationLooksDirectory && !rootCandidateIsDirectory) {
                continue;
            }
            if (mode == PBResolvedPathModeDestination && !rootCandidateIsDirectory) {
                return [[rootCandidateURL URLByDeletingLastPathComponent] path] ?: @"";
            }
            return rootCandidateURL.path ?: @"";
        }

        NSString *rootPath = rootURL.path ?: @"";
        candidateURL = PBStandardizedFileURL([rootPath stringByAppendingPathComponent:expanded]);
    }

    BOOL candidateIsDirectory = NO;
    if (PBURLExists(candidateURL, &candidateIsDirectory)) {
        if (sourceWantsDirectory && !candidateIsDirectory) {
            // Keep looking for a folder match when the input looks like a folder name.
        } else
        if (mode == PBResolvedPathModeDestination && !candidateIsDirectory) {
            return [[candidateURL URLByDeletingLastPathComponent] path] ?: @"";
        } else {
            return candidateURL.path ?: @"";
        }
    }

    NSString *searchName = PBLastPathComponentFromInput(expanded);
    for (NSURL *candidateRootURL in rootURLs) {
        NSURL *searchedURL =
            PBFindDescendantNamed(candidateRootURL,
                                  searchName,
                                  sourceWantsDirectory);
        if (searchedURL == nil) {
            continue;
        }

        BOOL searchedIsDirectory = NO;
        PBURLExists(searchedURL, &searchedIsDirectory);
        if (destinationLooksDirectory && !searchedIsDirectory) {
            continue;
        }
        if (mode == PBResolvedPathModeDestination && !searchedIsDirectory) {
            return [[searchedURL URLByDeletingLastPathComponent] path] ?: @"";
        }
        return searchedURL.path ?: @"";
    }

    if (mode == PBResolvedPathModeDestination) {
        if (destinationLooksDirectory) {
            return candidateURL.path ?: @"";
        }
        return PBResolvedDestinationFallbackPath(candidateURL, rootURL);
    }

    return candidateURL.path ?: @"";
}

static peanutbutter::StrengthPresetV2 PBStrengthFromTitle(NSString *title) {
    NSString *safeTitle = title ?: @"";
    if ([safeTitle localizedCaseInsensitiveContainsString:@"low"]) {
        return peanutbutter::StrengthPresetV2::kLow;
    }
    if ([safeTitle localizedCaseInsensitiveContainsString:@"medium"]) {
        return peanutbutter::StrengthPresetV2::kMedium;
    }
    return peanutbutter::StrengthPresetV2::kHigh;
}

static std::uint32_t PBBlockCountFromTitle(NSString *title) {
    NSString *safeTitle = title ?: @"";
    NSScanner *scanner = [NSScanner scannerWithString:safeTitle];
    NSInteger value = 0;
    if ([scanner scanInteger:&value] && value > 0) {
        return (std::uint32_t)value;
    }
    return peanutbutter::knobs::kDefaultBlocksPerArchiveV2;
}

static NSString *PBCurrentActionTitle(HomeContainerViewController *controller) {
    NSInteger selectedSegment =
        controller.homeToolViewTop.modeToggle.selectedSegment;
    if (selectedSegment >= 0 &&
        selectedSegment < controller.homeToolViewTop.modeToggle.segmentCount) {
        NSString *label = [controller.homeToolViewTop.modeToggle labelForSegment:selectedSegment];
        if (label.length > 0) {
            return label;
        }
    }
    return @"Bundle";
}

static NSString *PBActionTitleFromPrimaryAction(peanutbutter::EnginePrimaryActionV2 action,
                                                HomeContainerViewController *controller) {
    switch (action) {
        case peanutbutter::EnginePrimaryActionV2::kBundle:
            return @"Bundle";
        case peanutbutter::EnginePrimaryActionV2::kDecode:
            return @"Unbundle";
        case peanutbutter::EnginePrimaryActionV2::kManifest:
            return @"Read Manifest";
        case peanutbutter::EnginePrimaryActionV2::kRepair:
            return @"Repair";
        case peanutbutter::EnginePrimaryActionV2::kSanity:
            return @"Folder Compare";
        case peanutbutter::EnginePrimaryActionV2::kNone:
        default:
            return PBCurrentActionTitle(controller);
    }
}

static NSAlertStyle PBAlertStyleFromDialogKind(peanutbutter::UiDialogKindV2 kind) {
    switch (kind) {
        case peanutbutter::UiDialogKindV2::kWarning:
            return NSAlertStyleWarning;
        case peanutbutter::UiDialogKindV2::kError:
            return NSAlertStyleCritical;
        case peanutbutter::UiDialogKindV2::kInfo:
        default:
            return NSAlertStyleInformational;
    }
}

@interface AppShell ()
- (void)scheduleCommandBusDrain;
- (void)drainCommandBus;
- (void)applyCommandBusItem:(const peanutbutter::CommandBusItemV2&)item;
@end

namespace {

class AppShellExecutorDelegateV2 final : public peanutbutter::ArchiverExecutorDelegate {
public:
    explicit AppShellExecutorDelegateV2(AppShell *owner)
        : mOwner(owner) {}

    void OnArchiverExecutorItemsAvailable() override {
        if (mOwner != nil) {
            [mOwner scheduleCommandBusDrain];
        }
    }

private:
    AppShell *mOwner = nil;
};

}  // namespace

@implementation AppShell {
    __weak HomeContainerViewController *_homeContainerViewController;
    std::unique_ptr<peanutbutter::ArchiverExecutor> _executor;
    std::unique_ptr<AppShellExecutorDelegateV2> _executorDelegate;
    dispatch_queue_t _heartbeatQueue;
    dispatch_source_t _heartbeatTimer;
    std::atomic<bool> _commandBusDrainScheduled;
}

- (instancetype)initWithHomeContainerViewController:(HomeContainerViewController *)homeContainerViewController {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _homeContainerViewController = homeContainerViewController;
    _executor = std::make_unique<peanutbutter::ArchiverExecutor>();
    _executorDelegate = std::make_unique<AppShellExecutorDelegateV2>(self);
    _executor->SetDelegate(_executorDelegate.get());
    _commandBusDrainScheduled.store(false);
    return self;
}

- (void)dealloc {
    if (_executor != nullptr) {
        _executor->SetDelegate(nullptr);
        _executor->Dispose();
    }
    [self stopPolling];
}

- (void)startPolling {
    if (_heartbeatTimer != nil) {
        return;
    }

    if (_heartbeatQueue == nil) {
        _heartbeatQueue = dispatch_queue_create("com.peanutbutter.archiver.heartbeat",
                                                DISPATCH_QUEUE_SERIAL);
    }

    if (_heartbeatTimer == nil) {
        _heartbeatTimer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
                                                0,
                                                0,
                                                _heartbeatQueue);
        dispatch_source_set_timer(_heartbeatTimer,
                                  dispatch_time(DISPATCH_TIME_NOW, 0),
                                  static_cast<uint64_t>(NSEC_PER_SEC / 240.0),
                                  static_cast<uint64_t>(NSEC_PER_MSEC));
        __weak AppShell *weakSelf = self;
        dispatch_source_set_event_handler(_heartbeatTimer, ^{
            AppShell *strongSelf = weakSelf;
            if (strongSelf == nil) {
                return;
            }
            if (strongSelf->_executor != nullptr) {
                strongSelf->_executor->Heartbeat();
            }
        });
        dispatch_resume(_heartbeatTimer);
    }
}

- (void)stopPolling {
    if (_heartbeatTimer != nil) {
        dispatch_source_cancel(_heartbeatTimer);
        _heartbeatTimer = nil;
    }
    _heartbeatQueue = nil;
    _commandBusDrainScheduled.store(false);
}

- (void)enqueueBundleRequestWithSourceDirectory:(NSString *)sourceDirectory
                           destinationDirectory:(NSString *)destinationDirectory
                                     filePrefix:(NSString *)filePrefix
                                  repairEnabled:(BOOL)repairEnabled
                                    safeEnabled:(BOOL)safeEnabled
                              encryptionEnabled:(BOOL)encryptionEnabled
                           includePreviewEnabled:(BOOL)includePreviewEnabled
                                       password:(NSString *)password
                                 repairSizeTitle:(NSString *)repairSizeTitle
                                 blockCountTitle:(NSString *)blockCountTitle
                        encryptionStrengthTitle:(NSString *)encryptionStrengthTitle
                              tableStrengthTitle:(NSString *)tableStrengthTitle {
    (void)safeEnabled;
    peanutbutter::BundleRequestV2 request;
    NSString *normalizedSourceDirectory = PBTrimmedString(sourceDirectory);
    NSString *normalizedDestinationDirectory = PBTrimmedString(destinationDirectory);
    NSString *normalizedFilePrefix = PBTrimmedString(filePrefix);

    request.mSourceDirectory = PBStdStringFromNSString(normalizedSourceDirectory);
    request.mDestinationDirectory = PBStdStringFromNSString(normalizedDestinationDirectory);
    request.mFilePrefix = PBStdStringFromNSString(normalizedFilePrefix.length > 0 ? normalizedFilePrefix : @"archive");
    request.mRepairEnabled = repairEnabled;
    request.mRepairCoverage = PBRepairCoverageFromTitle(repairSizeTitle);
    request.mEncryptionEnabled = encryptionEnabled;
    request.mIncludePreviewManifest = includePreviewEnabled;
    request.mPassword = PBStdStringFromNSString(password);
    request.mBlockCount = PBBlockCountFromTitle(blockCountTitle);
    request.mEncryptionStrength = PBStrengthFromTitle(encryptionStrengthTitle);
    request.mTableStrength = PBStrengthFromTitle(tableStrengthTitle);

    if (_executor != nullptr) {
        _executor->EnqueueBundleRequest(request);
    }
}

- (void)enqueueUnbundleRequestWithSourcePath:(NSString *)sourcePath
                        destinationDirectory:(NSString *)destinationDirectory
                              recoverEnabled:(BOOL)recoverEnabled
                                    password:(NSString *)password {
    peanutbutter::DecodeRequestV2 request;
    request.mSourcePath =
        PBStdStringFromNSString(PBResolvePathString(sourcePath, PBResolvedPathModeSource));
    request.mDestinationDirectory =
        PBStdStringFromNSString(PBResolvePathString(destinationDirectory, PBResolvedPathModeDestination));
    request.mEncryptionEnabled = YES;
    request.mPassword = PBStdStringFromNSString(password);
    request.mIntent = recoverEnabled ? peanutbutter::DecodeIntentV2::kRecover
                                     : peanutbutter::DecodeIntentV2::kUnbundle;

    if (_executor != nullptr) {
        _executor->EnqueueDecodeRequest(request);
    }
}

- (void)enqueueManifestRequestWithSourcePath:(NSString *)sourcePath
                        destinationDirectory:(NSString *)destinationDirectory
                                    password:(NSString *)password {
    peanutbutter::DecodeRequestV2 request;
    request.mSourcePath =
        PBStdStringFromNSString(PBResolvePathString(sourcePath, PBResolvedPathModeSource));
    request.mDestinationDirectory =
        PBStdStringFromNSString(PBResolvePathString(destinationDirectory, PBResolvedPathModeDestination));
    request.mEncryptionEnabled = YES;
    request.mPassword = PBStdStringFromNSString(password);
    request.mIntent = peanutbutter::DecodeIntentV2::kManifest;

    if (_executor != nullptr) {
        _executor->EnqueueManifestRequest(request);
    }
}

- (void)enqueueRepairRequestWithSourcePath:(NSString *)sourcePath
                      destinationDirectory:(NSString *)destinationDirectory
                          aggressiveEnabled:(BOOL)aggressiveEnabled
                                  password:(NSString *)password {
    peanutbutter::RepairRequestV2 request;
    request.mSourcePath =
        PBStdStringFromNSString(PBResolvePathString(sourcePath, PBResolvedPathModeSource));
    request.mDestinationDirectory =
        PBStdStringFromNSString(PBResolvePathString(destinationDirectory, PBResolvedPathModeDestination));
    request.mEncryptionEnabled = YES;
    request.mAggressive = aggressiveEnabled;
    request.mPassword = PBStdStringFromNSString(password);

    if (_executor != nullptr) {
        _executor->EnqueueRepairRequest(request);
    }
}

- (void)enqueueSanityRequestWithLeftDirectory:(NSString *)leftDirectory
                               rightDirectory:(NSString *)rightDirectory
                                 ignoreHidden:(BOOL)ignoreHidden {
    peanutbutter::SanityRequestV2 request;
    request.mLeftDirectory =
        PBStdStringFromNSString(PBResolvePathString(leftDirectory, PBResolvedPathModeSource));
    request.mRightDirectory =
        PBStdStringFromNSString(PBResolvePathString(rightDirectory, PBResolvedPathModeSource));
    request.mIgnoreHidden = ignoreHidden;

    if (_executor != nullptr) {
        _executor->EnqueueSanityRequest(request);
    }
}

- (void)enqueueCancelRequest {
    if (_executor != nullptr) {
        _executor->EnqueueCancelRequest();
    }
}

- (void)setVerboseRuntimeEventsEnabled:(BOOL)enabled {
    if (_executor != nullptr) {
        _executor->SetCaptureVerboseRuntimeEvents(enabled);
    }
}

- (void)presentDialogRequest:(const peanutbutter::UiDialogRequestV2&)dialog {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = PBAlertStyleFromDialogKind(dialog.mKind);
    alert.messageText = PBNSStringFromStdString(dialog.mTitle);
    alert.informativeText = PBNSStringFromStdString(dialog.mMessage);
    [alert addButtonWithTitle:@"OK"];

    NSWindow *window = _homeContainerViewController.view.window;
    if (window != nil) {
        [alert beginSheetModalForWindow:window completionHandler:nil];
    } else {
        [alert runModal];
    }
}

- (void)presentPromptRequest:(const peanutbutter::UiPromptRequestV2&)prompt {
    peanutbutter::UiPromptRequestV2 promptCopy = prompt;
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = NSAlertStyleWarning;
    alert.messageText = PBNSStringFromStdString(promptCopy.mTitle);
    alert.informativeText = PBNSStringFromStdString(promptCopy.mMessage);

    NSString *primaryLabel = PBNSStringFromStdString(promptCopy.mPrimaryLabel);
    NSString *secondaryLabel = PBNSStringFromStdString(promptCopy.mSecondaryLabel);
    NSString *cancelLabel = PBNSStringFromStdString(promptCopy.mCancelLabel);
    NSButton *primaryButton = [alert addButtonWithTitle:(primaryLabel.length > 0 ? primaryLabel : @"Clear")];
    NSButton *secondaryButton = [alert addButtonWithTitle:(secondaryLabel.length > 0 ? secondaryLabel : @"Merge")];
    NSButton *cancelButton = [alert addButtonWithTitle:(cancelLabel.length > 0 ? cancelLabel : @"Cancel")];
    (void)primaryButton;
    (void)secondaryButton;
    (void)cancelButton;

    void (^responseHandler)(NSModalResponse) = ^(NSModalResponse response) {
        peanutbutter::UiPromptResponseV2 promptResponse;
        promptResponse.mPromptId = promptCopy.mPromptId;
        promptResponse.mKind = promptCopy.mKind;
        if (response == NSAlertFirstButtonReturn) {
            promptResponse.mChoice = peanutbutter::UiPromptChoiceV2::kClear;
        } else if (response == NSAlertSecondButtonReturn) {
            promptResponse.mChoice = peanutbutter::UiPromptChoiceV2::kMerge;
        } else {
            promptResponse.mChoice = peanutbutter::UiPromptChoiceV2::kCancel;
        }
        if (self->_executor != nullptr) {
            self->_executor->EnqueuePromptResponse(promptResponse);
        }
    };

    NSWindow *window = _homeContainerViewController.view.window;
    if (window != nil) {
        [alert beginSheetModalForWindow:window completionHandler:^(NSModalResponse response) {
            responseHandler(response);
        }];
    } else {
        responseHandler([alert runModal]);
    }
}

- (void)scheduleCommandBusDrain {
    if (_homeContainerViewController == nil) {
        return;
    }

    const bool alreadyScheduled =
        _commandBusDrainScheduled.exchange(true);
    if (alreadyScheduled) {
        return;
    }

    __weak AppShell *weakSelf = self;
    dispatch_async(dispatch_get_main_queue(), ^{
        AppShell *strongSelf = weakSelf;
        if (strongSelf == nil) {
            return;
        }
        strongSelf->_commandBusDrainScheduled.store(false);
        [strongSelf drainCommandBus];
    });
}

- (void)drainCommandBus {
    if (_homeContainerViewController == nil || _executor == nullptr) {
        return;
    }

    while (true) {
        const peanutbutter::CommandBusItemListV2 items = _executor->TakeItems();
        if (items.empty()) {
            break;
        }

        for (const peanutbutter::CommandBusItemV2& item : items) {
            [self applyCommandBusItem:item];
        }
    }
}

- (void)applyCommandBusItem:(const peanutbutter::CommandBusItemV2&)item {
    if (item.mType == peanutbutter::CommandBusItemTypeV2::kLog) {
        [self appendLogLine:PBNSStringFromStdString(item.mLog.mMessage)];
        return;
    }

    [self applyEvent:item.mEvent];
}

- (void)applyEvent:(const peanutbutter::EngineEventV2&)event {
    if (_homeContainerViewController == nil) {
        return;
    }

    switch (event.mType) {
        case peanutbutter::EngineEventTypeV2::kActionAccepted:
        case peanutbutter::EngineEventTypeV2::kActionRejected:
        case peanutbutter::EngineEventTypeV2::kActionCompleted:
        case peanutbutter::EngineEventTypeV2::kActionFailed:
        case peanutbutter::EngineEventTypeV2::kActionCanceled:
        case peanutbutter::EngineEventTypeV2::kCancelAccepted:
        case peanutbutter::EngineEventTypeV2::kCancelRejected:
            [self appendLogLine:PBNSStringFromStdString(event.mMessage)];
            break;
        case peanutbutter::EngineEventTypeV2::kUiStateChanged:
            [self applyUiEffect:event.mUiEffect snapshot:event.mSnapshot];
            break;
        case peanutbutter::EngineEventTypeV2::kLog:
            [self appendLogLine:PBNSStringFromStdString(event.mLogEntry.mMessage)];
            break;
        case peanutbutter::EngineEventTypeV2::kProgress:
            [self applyProgress:event.mProgress snapshot:event.mSnapshot];
            break;
        case peanutbutter::EngineEventTypeV2::kCheckpointRequested:
            if (PBShouldRenderRuntimeLog(event.mCheckpointRequest.mRuntimeEvent)) {
                [self appendLogLine:PBNSStringFromStdString(PBCompactCheckpointLine(event.mCheckpointRequest))];
            }
            if (_executor != nullptr) {
                _executor->ContinueCheckpoint(event.mCheckpointRequest.mCheckpointId);
            }
            break;
        case peanutbutter::EngineEventTypeV2::kRuntimeEvent:
            if (PBShouldRenderRuntimeLog(event.mRuntimeEvent)) {
                [self appendLogLine:PBNSStringFromStdString(PBCompactRuntimeEventLine(event.mRuntimeEvent))];
            }
            break;
    }

    BOOL isUiEnabled = !event.mSnapshot.mIsUiLocked;
    [_homeContainerViewController.homeHeaderView setButtonsEnabled:isUiEnabled];
    [_homeContainerViewController.homeActiveModeContainerView setBundleControlsEnabled:isUiEnabled];
    _homeContainerViewController.homeActiveModeContainerView.progressCancelButton.enabled =
        event.mSnapshot.mCurrentPrimaryAction != peanutbutter::EnginePrimaryActionV2::kNone &&
        !event.mSnapshot.mIsCancelPending;

    if (event.mType == peanutbutter::EngineEventTypeV2::kActionRejected &&
        !event.mSnapshot.mIsUiLocked &&
        event.mSnapshot.mCurrentPrimaryAction == peanutbutter::EnginePrimaryActionV2::kNone) {
        [_homeContainerViewController transitionToHomeState];
    }
}

- (void)applyUiEffect:(const peanutbutter::UiEffectV2&)effect
             snapshot:(const peanutbutter::EngineSnapshotV2&)snapshot {
    NSString *actionTitle =
        PBActionTitleFromPrimaryAction(snapshot.mCurrentPrimaryAction, _homeContainerViewController);
    switch (effect.mType) {
        case peanutbutter::UiEffectTypeV2::kShowLoading:
        case peanutbutter::UiEffectTypeV2::kUpdateLoading:
            [_homeContainerViewController transitionToLoadingStateWithTitle:actionTitle
                                                                     detail:PBNSStringFromStdString(effect.mLabel)
                                                                   fraction:0.0
                                                              cancelEnabled:(snapshot.mCurrentPrimaryAction != peanutbutter::EnginePrimaryActionV2::kNone &&
                                                                             !snapshot.mIsCancelPending)];
            break;
        case peanutbutter::UiEffectTypeV2::kHideLoading:
            if (!snapshot.mIsUiLocked) {
                [_homeContainerViewController transitionToHomeState];
            } else {
                [_homeContainerViewController.homeActiveModeContainerView setShowsProgressPanel:NO];
            }
            break;
        case peanutbutter::UiEffectTypeV2::kShowDialog:
            [self presentDialogRequest:effect.mDialog];
            break;
        case peanutbutter::UiEffectTypeV2::kShowPrompt:
            [self presentPromptRequest:effect.mPrompt];
            break;
    }
}

- (void)applyProgress:(const peanutbutter::ProgressSnapshotV2&)progress
             snapshot:(const peanutbutter::EngineSnapshotV2&)snapshot {
    NSString *actionTitle =
        PBActionTitleFromPrimaryAction(snapshot.mCurrentPrimaryAction, _homeContainerViewController);
    [_homeContainerViewController transitionToLoadingStateWithTitle:actionTitle
                                                             detail:PBNSStringFromStdString(progress.mLabel)
                                                           fraction:progress.mOverallFraction
                                                      cancelEnabled:(snapshot.mCurrentPrimaryAction != peanutbutter::EnginePrimaryActionV2::kNone &&
                                                                     !snapshot.mIsCancelPending)];
}

- (void)appendLogLine:(NSString *)line {
    [_homeContainerViewController.homeLogContainerView.homeLogView appendLine:line];
}

@end

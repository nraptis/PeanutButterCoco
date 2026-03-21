#import "AppShell.hpp"

#include <memory>

#import "../../Common/BundleRequest.hpp"
#import "../../Common/DecodeRequest.hpp"
#import "../../Common/RepairRequest.hpp"
#import "../../Common/SanityRequest.hpp"
#import "../../Engine/ArchiverEngine.hpp"
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
    return 4u;
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

@implementation AppShell {
    __weak HomeContainerViewController *_homeContainerViewController;
    std::unique_ptr<peanutbutter::ArchiverEngine> _engine;
    NSTimer *_pollTimer;
}

- (instancetype)initWithHomeContainerViewController:(HomeContainerViewController *)homeContainerViewController {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _homeContainerViewController = homeContainerViewController;
    _engine = std::make_unique<peanutbutter::ArchiverEngine>();
    return self;
}

- (void)startPolling {
    if (_pollTimer != nil) {
        return;
    }

    _pollTimer = [NSTimer scheduledTimerWithTimeInterval:(1.0 / 60.0)
                                                  target:self
                                                selector:@selector(handlePollTimer:)
                                                userInfo:nil
                                                 repeats:YES];
}

- (void)stopPolling {
    [_pollTimer invalidate];
    _pollTimer = nil;
}

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
                              tableStrengthTitle:(NSString *)tableStrengthTitle {
    peanutbutter::BundleRequestV2 request;
    NSString *normalizedSourceDirectory = PBTrimmedString(sourceDirectory);
    NSString *normalizedDestinationDirectory = PBTrimmedString(destinationDirectory);
    NSString *normalizedFilePrefix = PBTrimmedString(filePrefix);

    request.mSourceDirectory = PBStdStringFromNSString(normalizedSourceDirectory);
    request.mDestinationDirectory = PBStdStringFromNSString(normalizedDestinationDirectory);
    request.mFilePrefix = PBStdStringFromNSString(normalizedFilePrefix.length > 0 ? normalizedFilePrefix : @"archive");
    request.mRepairEnabled = repairEnabled;
    request.mSafeModeEnabled = safeEnabled;
    request.mEncryptionEnabled = encryptionEnabled;
    request.mIncludePreviewManifest = includePreviewEnabled;
    request.mPassword = PBStdStringFromNSString(password);
    request.mBlockCount = PBBlockCountFromTitle(blockCountTitle);
    request.mEncryptionStrength = PBStrengthFromTitle(encryptionStrengthTitle);
    request.mTableStrength = PBStrengthFromTitle(tableStrengthTitle);

    _engine->EnqueueBundleRequest(request);
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

    _engine->EnqueueDecodeRequest(request);
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

    _engine->EnqueueManifestRequest(request);
}

- (void)enqueueRepairRequestWithSourcePath:(NSString *)sourcePath
                      destinationDirectory:(NSString *)destinationDirectory
                                  password:(NSString *)password {
    peanutbutter::RepairRequestV2 request;
    request.mSourcePath =
        PBStdStringFromNSString(PBResolvePathString(sourcePath, PBResolvedPathModeSource));
    request.mDestinationDirectory =
        PBStdStringFromNSString(PBResolvePathString(destinationDirectory, PBResolvedPathModeDestination));
    request.mEncryptionEnabled = YES;
    request.mPassword = PBStdStringFromNSString(password);

    _engine->EnqueueRepairRequest(request);
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

    _engine->EnqueueSanityRequest(request);
}

- (void)enqueueCancelRequest {
    _engine->EnqueueCancelRequest();
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
        self->_engine->EnqueuePromptResponse(promptResponse);
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

- (void)handlePollTimer:(NSTimer *)timer {
    (void)timer;
    if (_homeContainerViewController == nil) {
        return;
    }

    peanutbutter::EngineEventListV2 events = _engine->Poll();
    [self applyEventBatch:events];
}

- (void)applyEventBatch:(const peanutbutter::EngineEventListV2&)events {
    if (_homeContainerViewController == nil || events.empty()) {
        return;
    }

    std::optional<peanutbutter::EngineSnapshotV2> finalSnapshot;
    std::optional<peanutbutter::UiEffectV2> finalLoadingEffect;
    std::optional<peanutbutter::EngineSnapshotV2> finalLoadingSnapshot;
    std::optional<peanutbutter::ProgressSnapshotV2> finalProgress;
    std::optional<peanutbutter::EngineSnapshotV2> finalProgressSnapshot;
    std::optional<peanutbutter::UiDialogRequestV2> finalDialog;
    std::optional<peanutbutter::UiPromptRequestV2> finalPrompt;

    for (const peanutbutter::EngineEventV2& event : events) {
        finalSnapshot = event.mSnapshot;

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
                switch (event.mUiEffect.mType) {
                    case peanutbutter::UiEffectTypeV2::kShowLoading:
                    case peanutbutter::UiEffectTypeV2::kUpdateLoading:
                    case peanutbutter::UiEffectTypeV2::kHideLoading:
                        finalLoadingEffect = event.mUiEffect;
                        finalLoadingSnapshot = event.mSnapshot;
                        break;
                    case peanutbutter::UiEffectTypeV2::kShowDialog:
                        finalDialog = event.mUiEffect.mDialog;
                        finalPrompt.reset();
                        break;
                    case peanutbutter::UiEffectTypeV2::kShowPrompt:
                        finalPrompt = event.mUiEffect.mPrompt;
                        finalDialog.reset();
                        break;
                }
                break;
            case peanutbutter::EngineEventTypeV2::kLog:
                [self appendLogLine:PBNSStringFromStdString(event.mLogEntry.mMessage)];
                break;
            case peanutbutter::EngineEventTypeV2::kProgress:
                finalProgress = event.mProgress;
                finalProgressSnapshot = event.mSnapshot;
                break;
        }
    }

    peanutbutter::EngineSnapshotV2 snapshot = finalSnapshot.value_or(peanutbutter::EngineSnapshotV2{});
    if (!snapshot.mIsUiLocked) {
        [_homeContainerViewController transitionToHomeState];
    } else if (finalProgress.has_value()) {
        peanutbutter::ProgressSnapshotV2 progress = *finalProgress;
        NSString *actionTitle =
            PBActionTitleFromPrimaryAction(snapshot.mCurrentPrimaryAction, _homeContainerViewController);
        [_homeContainerViewController transitionToLoadingStateWithTitle:actionTitle
                                                                 detail:PBNSStringFromStdString(progress.mLabel)
                                                               fraction:progress.mOverallFraction
                                                          cancelEnabled:!snapshot.mIsCancelPending];
    } else if (finalLoadingEffect.has_value() &&
               finalLoadingEffect->mType != peanutbutter::UiEffectTypeV2::kHideLoading) {
        peanutbutter::UiEffectV2 effect = *finalLoadingEffect;
        NSString *actionTitle =
            PBActionTitleFromPrimaryAction(snapshot.mCurrentPrimaryAction, _homeContainerViewController);
        [_homeContainerViewController transitionToLoadingStateWithTitle:actionTitle
                                                                 detail:PBNSStringFromStdString(effect.mLabel)
                                                               fraction:0.0
                                                          cancelEnabled:!snapshot.mIsCancelPending];
    } else {
        NSString *actionTitle =
            PBActionTitleFromPrimaryAction(snapshot.mCurrentPrimaryAction, _homeContainerViewController);
        [_homeContainerViewController transitionToGhostStateWithTitle:actionTitle
                                                               detail:@"Preparing..."];
    }

    if (finalDialog.has_value()) {
        peanutbutter::UiDialogRequestV2 dialog = *finalDialog;
        [self presentDialogRequest:dialog];
    } else if (finalPrompt.has_value()) {
        peanutbutter::UiPromptRequestV2 prompt = *finalPrompt;
        [self presentPromptRequest:prompt];
    }
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
    }

    BOOL isUiEnabled = !event.mSnapshot.mIsUiLocked;
    [_homeContainerViewController.homeHeaderView setButtonsEnabled:isUiEnabled];
    [_homeContainerViewController.homeActiveModeContainerView setBundleControlsEnabled:isUiEnabled];
    _homeContainerViewController.homeActiveModeContainerView.progressCancelButton.enabled =
        event.mSnapshot.mCurrentPrimaryAction != peanutbutter::EnginePrimaryActionV2::kNone &&
        !event.mSnapshot.mIsCancelPending;
}

- (void)applyUiEffect:(const peanutbutter::UiEffectV2&)effect
             snapshot:(const peanutbutter::EngineSnapshotV2&)snapshot {
    NSString *actionTitle =
        PBActionTitleFromPrimaryAction(snapshot.mCurrentPrimaryAction, _homeContainerViewController);
    switch (effect.mType) {
        case peanutbutter::UiEffectTypeV2::kShowLoading:
        case peanutbutter::UiEffectTypeV2::kUpdateLoading:
            [_homeContainerViewController.homeActiveModeContainerView setShowsProgressPanel:YES];
            [_homeContainerViewController.homeActiveModeContainerView
                updateProgressTitle:actionTitle
                              detail:PBNSStringFromStdString(effect.mLabel)
                            fraction:0.0];
            break;
        case peanutbutter::UiEffectTypeV2::kHideLoading:
            [_homeContainerViewController.homeActiveModeContainerView setShowsProgressPanel:NO];
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
    [_homeContainerViewController.homeActiveModeContainerView setShowsProgressPanel:YES];
    [_homeContainerViewController.homeActiveModeContainerView
        updateProgressTitle:actionTitle
                      detail:PBNSStringFromStdString(progress.mLabel)
                    fraction:progress.mOverallFraction];
}

- (void)appendLogLine:(NSString *)line {
    [_homeContainerViewController.homeLogContainerView.homeLogView appendLine:line];
}

@end

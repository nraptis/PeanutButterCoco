#import "MockAppShell.hpp"

#include <memory>

#import "../../Engine/Mock/MockEngine.hpp"
#import "../ViewControllers/HomeContainerViewController.hpp"
#import "../Views/HomeActiveModeContainerView.hpp"
#import "../Views/HomeFooterView.hpp"
#import "../Views/HomeHeaderView.hpp"
#import "../Views/HomeLogContainerView.hpp"
#import "../Views/HomeLogView.hpp"
#import "../Views/HomeToolViewSplitter.hpp"
#import "../Views/ProgressPanelView.hpp"

static NSString *NSStringFromStdString(const std::string& value) {
    return [NSString stringWithUTF8String:value.c_str()] ?: @"";
}

@implementation MockAppShell {
    __weak HomeContainerViewController *_homeContainerViewController;
    std::unique_ptr<peanutbutter::MockEngine> _mockEngine;
    NSTimer *_pollTimer;
}

- (instancetype)initWithHomeContainerViewController:(HomeContainerViewController *)homeContainerViewController {
    self = [super init];
    if (self == nil) {
        return nil;
    }

    _homeContainerViewController = homeContainerViewController;
    _mockEngine = std::make_unique<peanutbutter::MockEngine>();
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

- (void)enqueueGreenDelaySucceed {
    _mockEngine->EnqueueScenario(peanutbutter::MockScenarioV2::kGreenDelaySucceed);
}

- (void)enqueueGreenDelayFail {
    _mockEngine->EnqueueScenario(peanutbutter::MockScenarioV2::kGreenDelayFail);
}

- (void)enqueueYellowDelaySucceed {
    _mockEngine->EnqueueScenario(peanutbutter::MockScenarioV2::kYellowDelaySucceed);
}

- (void)enqueueYellowDelayFail {
    _mockEngine->EnqueueScenario(peanutbutter::MockScenarioV2::kYellowDelayFail);
}

- (void)enqueueRed {
    _mockEngine->EnqueueScenario(peanutbutter::MockScenarioV2::kRed);
}

- (void)enqueueCancelRequest {
    _mockEngine->EnqueueCancelRequest();
}

- (BOOL)hasActivePrimaryAction {
    return _mockEngine->Snapshot().mCurrentPrimaryAction != peanutbutter::EnginePrimaryActionV2::kNone;
}

- (void)presentDialogRequest:(const peanutbutter::UiDialogRequestV2&)dialog {
    NSAlert *alert = [[NSAlert alloc] init];
    alert.alertStyle = (dialog.mKind == peanutbutter::UiDialogKindV2::kError)
                           ? NSAlertStyleWarning
                           : NSAlertStyleInformational;
    alert.messageText = NSStringFromStdString(dialog.mTitle);
    alert.informativeText = NSStringFromStdString(dialog.mMessage);
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
    alert.messageText = NSStringFromStdString(promptCopy.mTitle);
    alert.informativeText = NSStringFromStdString(promptCopy.mMessage);

    NSString *primaryLabel = NSStringFromStdString(promptCopy.mPrimaryLabel);
    NSString *secondaryLabel = NSStringFromStdString(promptCopy.mSecondaryLabel);
    NSString *cancelLabel = NSStringFromStdString(promptCopy.mCancelLabel);
    [alert addButtonWithTitle:(primaryLabel.length > 0 ? primaryLabel : @"Clear")];
    [alert addButtonWithTitle:(secondaryLabel.length > 0 ? secondaryLabel : @"Merge")];
    [alert addButtonWithTitle:(cancelLabel.length > 0 ? cancelLabel : @"Cancel")];

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
        self->_mockEngine->EnqueuePromptResponse(promptResponse);
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

    peanutbutter::EngineEventListV2 events = _mockEngine->Poll();
    [self applyEventBatch:events];
}

- (void)applyEventBatch:(const peanutbutter::EngineEventListV2&)events {
    if (_homeContainerViewController == nil || events.empty()) {
        return;
    }

    std::optional<peanutbutter::EngineSnapshotV2> finalSnapshot;
    std::optional<peanutbutter::UiEffectV2> finalLoadingEffect;
    std::optional<peanutbutter::ProgressSnapshotV2> finalProgress;
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
                [self appendLogLine:NSStringFromStdString(event.mMessage)];
                break;
            case peanutbutter::EngineEventTypeV2::kUiStateChanged:
                switch (event.mUiEffect.mType) {
                    case peanutbutter::UiEffectTypeV2::kShowLoading:
                    case peanutbutter::UiEffectTypeV2::kUpdateLoading:
                    case peanutbutter::UiEffectTypeV2::kHideLoading:
                        finalLoadingEffect = event.mUiEffect;
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
                [self appendLogLine:NSStringFromStdString(event.mLogEntry.mMessage)];
                break;
            case peanutbutter::EngineEventTypeV2::kProgress:
                finalProgress = event.mProgress;
                break;
        }
    }

    peanutbutter::EngineSnapshotV2 snapshot = finalSnapshot.value_or(peanutbutter::EngineSnapshotV2{});
    if (!snapshot.mIsUiLocked) {
        [_homeContainerViewController transitionToHomeState];
    } else if (finalProgress.has_value()) {
        peanutbutter::ProgressSnapshotV2 progress = *finalProgress;
        [_homeContainerViewController transitionToLoadingStateWithTitle:@"Mock Engine Running"
                                                                 detail:NSStringFromStdString(progress.mLabel)
                                                               fraction:progress.mOverallFraction
                                                          cancelEnabled:!snapshot.mIsCancelPending];
    } else if (finalLoadingEffect.has_value() &&
               finalLoadingEffect->mType != peanutbutter::UiEffectTypeV2::kHideLoading) {
        peanutbutter::UiEffectV2 effect = *finalLoadingEffect;
        [_homeContainerViewController transitionToLoadingStateWithTitle:@"Mock Engine Running"
                                                                 detail:NSStringFromStdString(effect.mLabel)
                                                               fraction:0.0
                                                          cancelEnabled:!snapshot.mIsCancelPending];
    } else {
        [_homeContainerViewController transitionToGhostStateWithTitle:@"Mock Engine Running"
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
            [self appendLogLine:NSStringFromStdString(event.mMessage)];
            break;
        case peanutbutter::EngineEventTypeV2::kUiStateChanged:
            [self applyUiEffect:event.mUiEffect];
            break;
        case peanutbutter::EngineEventTypeV2::kLog:
            [self appendLogLine:NSStringFromStdString(event.mLogEntry.mMessage)];
            break;
        case peanutbutter::EngineEventTypeV2::kProgress:
            [self applyProgress:event.mProgress];
            break;
    }

    [_homeContainerViewController.homeHeaderView setButtonsEnabled:!event.mSnapshot.mIsUiLocked];
    _homeContainerViewController.homeActiveModeContainerView.progressCancelButton.enabled =
        event.mSnapshot.mCurrentPrimaryAction != peanutbutter::EnginePrimaryActionV2::kNone &&
        !event.mSnapshot.mIsCancelPending;
}

- (void)applyUiEffect:(const peanutbutter::UiEffectV2&)effect {
    switch (effect.mType) {
        case peanutbutter::UiEffectTypeV2::kShowLoading:
        case peanutbutter::UiEffectTypeV2::kUpdateLoading:
            [_homeContainerViewController.homeActiveModeContainerView setShowsProgressPanel:YES];
            [_homeContainerViewController.homeActiveModeContainerView
                updateProgressTitle:@"Mock Engine Running"
                              detail:NSStringFromStdString(effect.mLabel)
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

- (void)applyProgress:(const peanutbutter::ProgressSnapshotV2&)progress {
    [_homeContainerViewController.homeActiveModeContainerView setShowsProgressPanel:YES];
    [_homeContainerViewController.homeActiveModeContainerView
        updateProgressTitle:@"Mock Engine Running"
                      detail:NSStringFromStdString(progress.mLabel)
                    fraction:progress.mOverallFraction];
}

- (void)appendLogLine:(NSString *)line {
    [_homeContainerViewController.homeLogContainerView.homeLogView appendLine:line];
}

@end

#include "Screens/Connection/DivineBeastsErrorReconnectScreen.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Localization/DivineBeastsUILocalization.h"
#include "ViewModels/DivineBeastsUIViewModel.h"

UDivineBeastsUIViewModel*
UDivineBeastsErrorReconnectScreen::GetProjectViewModel() const
{
    return Cast<UDivineBeastsUIViewModel>(GetViewModel());
}

FText UDivineBeastsErrorReconnectScreen::GetDisplayErrorText() const
{
    // 优先展示业务已生成的安全错误说明；内部错误码必须通过受控映射。
    const UDivineBeastsUIViewModel* LocalViewModel = GetProjectViewModel();
    if (LocalViewModel)
    {
        const FDivineBeastsUIViewState& State = LocalViewModel->GetStateRef();
        if (!State.ErrorText.IsEmpty())
        {
            return State.ErrorText;
        }
        if (!State.ErrorCode.IsNone())
        {
            return FDivineBeastsUILocalization::ErrorCodeToText(State.ErrorCode);
        }
        if (!LocalViewModel->GetLastCommandErrorCode().IsNone())
        {
            return FDivineBeastsUILocalization::ErrorCodeToText(
                LocalViewModel->GetLastCommandErrorCode());
        }
    }
    return NSLOCTEXT(
        "DivineBeastsUI", "ErrorReconnectGeneric",
        "连接或加载失败，请检查网络后重试。");
}

void UDivineBeastsErrorReconnectScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    if (!bRetryEventsBound)
    {
        if (RetryButton)
        {
            RetryButton->OnClicked.AddUniqueDynamic(
                this, &UDivineBeastsErrorReconnectScreen::HandleRetryClicked);
        }
        if (UDivineBeastsUIViewModel* LocalViewModel = GetProjectViewModel())
        {
            LocalViewModel->OnViewStateChanged.AddUniqueDynamic(
                this, &UDivineBeastsErrorReconnectScreen::HandleViewStateChanged);
            LocalViewModel->OnCommandCompleted.AddUniqueDynamic(
                this, &UDivineBeastsErrorReconnectScreen::HandleRetryCompleted);
        }
        bRetryEventsBound = true;
    }
    RefreshErrorPresentation();
}

void UDivineBeastsErrorReconnectScreen::NativeOnDeactivated()
{
    if (bRetryEventsBound)
    {
        if (RetryButton)
        {
            RetryButton->OnClicked.RemoveDynamic(
                this, &UDivineBeastsErrorReconnectScreen::HandleRetryClicked);
        }
        if (UDivineBeastsUIViewModel* LocalViewModel = GetProjectViewModel())
        {
            LocalViewModel->OnViewStateChanged.RemoveDynamic(
                this, &UDivineBeastsErrorReconnectScreen::HandleViewStateChanged);
            LocalViewModel->OnCommandCompleted.RemoveDynamic(
                this, &UDivineBeastsErrorReconnectScreen::HandleRetryCompleted);
        }
    }
    bRetryEventsBound = false;
    ActiveRetryRequestId.Invalidate();
    bSubmittingRetry = false;
    bRetryCompletedDuringSubmit = false;
    // 基类负责终止项目LocalViewModel页面代次并取消归本页的异步命令。
    Super::NativeOnDeactivated();
}

void UDivineBeastsErrorReconnectScreen::RefreshErrorPresentation()
{
    UDivineBeastsUIViewModel* LocalViewModel = GetProjectViewModel();
    if (ErrorText)
    {
        ErrorText->SetText(GetDisplayErrorText());
    }

    const FDivineBeastsUIViewState* State =
        LocalViewModel ? &LocalViewModel->GetStateRef() : nullptr;
    const bool bCanRetry =
        State &&
        !State->bBusy &&
        State->AllowedCommands.Contains(TEXT("Retry")) &&
        !ActiveRetryRequestId.IsValid() &&
        !bSubmittingRetry;

    if (RetryButton)
    {
        RetryButton->SetIsEnabled(bCanRetry);
    }
}

void UDivineBeastsErrorReconnectScreen::HandleRetryClicked()
{
    UDivineBeastsUIViewModel* LocalViewModel = GetProjectViewModel();
    if (!LocalViewModel || ActiveRetryRequestId.IsValid() || bSubmittingRetry)
    {
        return;
    }
    const FDivineBeastsUIViewState& State = LocalViewModel->GetStateRef();
    if (State.bBusy || !State.AllowedCommands.Contains(TEXT("Retry")))
    {
        RefreshErrorPresentation();
        return;
    }

    // 同步拒绝可能在Retry()返回前广播完成；防止误保留已结束的请求身份。
    bSubmittingRetry = true;
    bRetryCompletedDuringSubmit = false;
    const FGuid SubmittedId = LocalViewModel->Retry();
    bSubmittingRetry = false;
    ActiveRetryRequestId =
        SubmittedId.IsValid() && !bRetryCompletedDuringSubmit
            ? SubmittedId : FGuid();
    RefreshErrorPresentation();
}

void UDivineBeastsErrorReconnectScreen::HandleViewStateChanged(int32, int32)
{
    RefreshErrorPresentation();
}

void UDivineBeastsErrorReconnectScreen::HandleRetryCompleted(
    FGuid RequestId, FName)
{
    if (bSubmittingRetry)
    {
        bRetryCompletedDuringSubmit = true;
    }
    if (ActiveRetryRequestId.IsValid() &&
        ActiveRetryRequestId != RequestId)
    {
        return;
    }
    ActiveRetryRequestId.Invalidate();
    RefreshErrorPresentation();
}

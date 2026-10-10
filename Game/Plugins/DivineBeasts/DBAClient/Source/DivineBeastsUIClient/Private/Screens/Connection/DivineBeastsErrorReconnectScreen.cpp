// 错误/重连页只提交既有Retry意图；平台负责激活/状态订阅，本页精确拥有按钮及VM命令通知。
// GT按VM/Page/作用域/提交代次处理同步重入；保留原反射函数与资产BindWidget身份，不改网络策略。
#include "Screens/Connection/DivineBeastsErrorReconnectScreen.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Localization/DivineBeastsUILocalization.h"
#include "ViewModels/DivineBeastsUIViewModel.h"
#include "UObject/StrongObjectPtr.h"

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

void UDivineBeastsErrorReconnectScreen::BindUIEvents()
{
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Selected(GetProjectViewModel());
    const int32 PageGeneration = Selected.IsValid() ? Selected->GetPageGeneration() : 0;
    Super::BindUIEvents();
    if (!IsActivated() || GetProjectViewModel() != Selected.Get() ||
        (Selected.IsValid() && (!Selected->IsPageActive() || Selected->GetPageGeneration() != PageGeneration))) return;
    ReconcileRetryBindings();
}

void UDivineBeastsErrorReconnectScreen::UnbindUIEvents()
{
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Previous(BoundRetryViewModel);
    // 先撤按钮、命令和提交资格，再允许父类取消命令触发后继激活；外部返回后不再写这些成员。
    ++RetryScopeGeneration;
    ++RetrySubmissionGeneration;
    bRetryEventsBound = false;
    BoundRetryViewModel = nullptr;
    BoundRetryPageGeneration = 0;
    ActiveRetryRequestId.Invalidate();
    bSubmittingRetry = false;
    CompletedRequestsDuringSubmit.Reset();
    if (RetryButton) RetryButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRetryClicked);
    if (Previous.IsValid()) Previous->OnCommandCompleted.RemoveDynamic(this, &ThisClass::HandleRetryCompleted);
    Super::UnbindUIEvents();
}

bool UDivineBeastsErrorReconnectScreen::IsRetryScopeCurrent() const
{
    return IsActivated() && bRetryEventsBound && BoundRetryViewModel &&
        GetProjectViewModel() == BoundRetryViewModel && BoundRetryViewModel->IsPageActive() &&
        BoundRetryViewModel->GetPageGeneration() == BoundRetryPageGeneration;
}

bool UDivineBeastsErrorReconnectScreen::ReconcileRetryBindings()
{
    if (!IsActivated()) return false;
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Selected(GetProjectViewModel());
    if (Selected.IsValid() && !Selected->IsPageActive()) return false;
    if (!bRetryEventsBound)
    {
        bRetryEventsBound = true;
        if (RetryButton) RetryButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
    }
    const int32 PageGeneration = Selected.IsValid() ? Selected->GetPageGeneration() : 0;
    if (BoundRetryViewModel != Selected.Get() || BoundRetryPageGeneration != PageGeneration)
    {
        const TStrongObjectPtr<UDivineBeastsUIViewModel> Previous(BoundRetryViewModel);
        ++RetryScopeGeneration;
        ++RetrySubmissionGeneration;
        BoundRetryViewModel = Selected.Get();
        BoundRetryPageGeneration = PageGeneration;
        ActiveRetryRequestId.Invalidate();
        bSubmittingRetry = false;
        CompletedRequestsDuringSubmit.Reset();
        if (Previous.IsValid()) Previous->OnCommandCompleted.RemoveDynamic(this, &ThisClass::HandleRetryCompleted);
        if (Selected.IsValid()) Selected->OnCommandCompleted.AddUniqueDynamic(this, &ThisClass::HandleRetryCompleted);
    }
    return IsRetryScopeCurrent();
}

void UDivineBeastsErrorReconnectScreen::RefreshInitialState()
{
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Selected(GetProjectViewModel());
    const int32 PageGeneration = Selected.IsValid() ? Selected->GetPageGeneration() : 0;
    Super::RefreshInitialState();
    if (!IsActivated() || GetProjectViewModel() != Selected.Get() ||
        (Selected.IsValid() && (!Selected->IsPageActive() || Selected->GetPageGeneration() != PageGeneration))) return;
    ReconcileRetryBindings();
    RefreshErrorPresentation();
}

void UDivineBeastsErrorReconnectScreen::OnViewModelStateChanged(int32 Revision, int32 PageGeneration)
{
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Selected(GetProjectViewModel());
    Super::OnViewModelStateChanged(Revision, PageGeneration);
    if (!IsActivated() || GetProjectViewModel() != Selected.Get() || !Selected.IsValid() ||
        !Selected->IsPageActive() || Selected->GetPageGeneration() != PageGeneration) return;
    ReconcileRetryBindings();
    HandleViewStateChanged(Revision, PageGeneration);
}

void UDivineBeastsErrorReconnectScreen::RefreshErrorPresentation()
{
    if (!IsActivated()) return;
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
    if (!IsRetryScopeCurrent() || ActiveRetryRequestId.IsValid() || bSubmittingRetry) return;
    const TStrongObjectPtr<UDivineBeastsErrorReconnectScreen> KeepScreen(this);
    const TStrongObjectPtr<UDivineBeastsUIViewModel> Selected(BoundRetryViewModel);
    const FDivineBeastsUIViewState State = Selected->GetStateRef();
    if (State.bBusy || !State.AllowedCommands.Contains(TEXT("Retry")))
    {
        RefreshErrorPresentation();
        return;
    }
    const uint64 ScopeGeneration = RetryScopeGeneration;
    const uint64 SubmissionGeneration = ++RetrySubmissionGeneration;
    const int32 PageGeneration = BoundRetryPageGeneration;
    bSubmittingRetry = true;
    CompletedRequestsDuringSubmit.Reset();
    // Retry可同步拒绝、换VM或关闭/重开页面；捕获资格在返回前完成一次复查，旧栈不得尾写新请求。
    const FGuid SubmittedId = Selected->Retry();
    if (!IsRetryScopeCurrent() || RetryScopeGeneration != ScopeGeneration ||
        RetrySubmissionGeneration != SubmissionGeneration || BoundRetryViewModel != Selected.Get() ||
        BoundRetryPageGeneration != PageGeneration || !bSubmittingRetry) return;
    bSubmittingRetry = false;
    ActiveRetryRequestId = SubmittedId.IsValid() && !CompletedRequestsDuringSubmit.Contains(SubmittedId)
        ? SubmittedId : FGuid();
    CompletedRequestsDuringSubmit.Reset();
    RefreshErrorPresentation();
}

void UDivineBeastsErrorReconnectScreen::HandleViewStateChanged(int32, int32 PageGeneration)
{
    if (!IsRetryScopeCurrent() || BoundRetryPageGeneration != PageGeneration) return;
    RefreshErrorPresentation();
}

void UDivineBeastsErrorReconnectScreen::HandleRetryCompleted(FGuid RequestId, FName)
{
    if (!IsRetryScopeCurrent()) return;
    if (bSubmittingRetry)
    {
        CompletedRequestsDuringSubmit.Add(RequestId);
        return; // 尚未知道Retry返回身份，不能用其他命令完成伪终结本次提交。
    }
    if (!ActiveRetryRequestId.IsValid() || ActiveRetryRequestId != RequestId) return;
    ActiveRetryRequestId.Invalidate();
    RefreshErrorPresentation();
}

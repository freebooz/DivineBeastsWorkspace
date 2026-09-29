#include "Screens/Login/DivineBeastsLoginScreen.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Localization/DivineBeastsUILocalization.h"
#include "ViewModels/Login/DivineBeastsLoginViewModel.h"

UDivineBeastsLoginViewModel*
UDivineBeastsLoginScreen::GetLoginViewModel() const
{
    // 页面不缓存第二份 ViewModel 指针，避免状态所有权重复。
    return Cast<UDivineBeastsLoginViewModel>(GetViewModel());
}

void UDivineBeastsLoginScreen::NativeOnActivated()
{
    Super::NativeOnActivated();
    BindLoginEvents();
    RefreshLoginPresentation();
}

void UDivineBeastsLoginScreen::NativeOnDeactivated()
{
    // 先解绑本页面事件再结束ViewModel页面生命周期，避免退出过程中产生无意义刷新。
    UnbindLoginEvents();
    ActiveLoginRequestId.Invalidate();
    ClearSensitiveInput();
    Super::NativeOnDeactivated();
}

void UDivineBeastsLoginScreen::BindLoginEvents()
{
    if (bLoginEventsBound)
    {
        return;
    }

    if (LoginButton)
    {
        LoginButton->OnClicked.AddUniqueDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleLoginClicked);
    }
    if (AccountInput)
    {
        AccountInput->OnTextChanged.AddUniqueDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleCredentialTextChanged);
    }
    if (PasswordInput)
    {
        PasswordInput->OnTextChanged.AddUniqueDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleCredentialTextChanged);
    }
    if (UDivineBeastsLoginViewModel* LoginViewModel = GetLoginViewModel())
    {
        LoginViewModel->OnViewStateChanged.AddUniqueDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleViewStateChanged);
        LoginViewModel->OnCommandCompleted.AddUniqueDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleCommandCompleted);
    }
    bLoginEventsBound = true;
}

void UDivineBeastsLoginScreen::UnbindLoginEvents()
{
    if (!bLoginEventsBound)
    {
        return;
    }

    if (LoginButton)
    {
        LoginButton->OnClicked.RemoveDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleLoginClicked);
    }
    if (AccountInput)
    {
        AccountInput->OnTextChanged.RemoveDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleCredentialTextChanged);
    }
    if (PasswordInput)
    {
        PasswordInput->OnTextChanged.RemoveDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleCredentialTextChanged);
    }
    if (UDivineBeastsLoginViewModel* LoginViewModel = GetLoginViewModel())
    {
        LoginViewModel->OnViewStateChanged.RemoveDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleViewStateChanged);
        LoginViewModel->OnCommandCompleted.RemoveDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleCommandCompleted);
    }
    bLoginEventsBound = false;
}

void UDivineBeastsLoginScreen::RefreshLoginPresentation()
{
    UDivineBeastsLoginViewModel* LoginViewModel = GetLoginViewModel();
    const bool bHasCredentials =
        AccountInput &&
        PasswordInput &&
        !AccountInput->GetText().ToString().TrimStartAndEnd().IsEmpty() &&
        !PasswordInput->GetText().IsEmpty();
    const bool bCanSubmit =
        LoginViewModel &&
        LoginViewModel->CanSubmitCredentials() &&
        bHasCredentials &&
        !ActiveLoginRequestId.IsValid();

    if (LoginButton)
    {
        LoginButton->SetIsEnabled(bCanSubmit);
    }

    const FDivineBeastsUIViewState* State =
        LoginViewModel ? &LoginViewModel->GetStateRef() : nullptr;
    const bool bBusy =
        ActiveLoginRequestId.IsValid() ||
        (State && State->bBusy);
    if (BusyIndicator)
    {
        BusyIndicator->SetVisibility(
            bBusy
                ? ESlateVisibility::SelfHitTestInvisible
                : ESlateVisibility::Collapsed);
    }

    if (MaintenanceText)
    {
        MaintenanceText->SetVisibility(
            State && State->bMaintenance
                ? ESlateVisibility::SelfHitTestInvisible
                : ESlateVisibility::Collapsed);
    }

    if (ErrorText)
    {
        FText Message = State ? State->ErrorText : FText::GetEmpty();
        if (Message.IsEmpty() && LoginViewModel)
        {
            const FName ErrorCode = LoginViewModel->GetLastCommandErrorCode();
            if (!ErrorCode.IsNone())
            {
                Message = FDivineBeastsUILocalization::ErrorCodeToText(
                    ErrorCode);
            }
        }
        ErrorText->SetText(Message);
        ErrorText->SetVisibility(
            Message.IsEmpty()
                ? ESlateVisibility::Collapsed
                : ESlateVisibility::SelfHitTestInvisible);
    }
}

void UDivineBeastsLoginScreen::ClearSensitiveInput()
{
    if (PasswordInput)
    {
        // 密码只允许作为一次命令参数存在；提交或离开页面后立即从Widget内存清空。
        PasswordInput->SetText(FText::GetEmpty());
    }
}

void UDivineBeastsLoginScreen::HandleLoginClicked()
{
    UDivineBeastsLoginViewModel* LoginViewModel = GetLoginViewModel();
    if (!LoginViewModel ||
        !LoginViewModel->CanSubmitCredentials() ||
        ActiveLoginRequestId.IsValid() ||
        !AccountInput ||
        !PasswordInput)
    {
        RefreshLoginPresentation();
        return;
    }

    const FString LoginName =
        AccountInput->GetText().ToString().TrimStartAndEnd();
    const FString Password = PasswordInput->GetText().ToString();
    if (LoginName.IsEmpty() || Password.IsEmpty())
    {
        RefreshLoginPresentation();
        return;
    }

    bSubmittingLogin = true;
    bLoginCompletedDuringSubmit = false;
    const FGuid SubmittedRequestId =
        LoginViewModel->Login(LoginName, Password);
    bSubmittingLogin = false;
    ActiveLoginRequestId =
        SubmittedRequestId.IsValid() && !bLoginCompletedDuringSubmit
            ? SubmittedRequestId
            : FGuid();
    ClearSensitiveInput();
    RefreshLoginPresentation();
}

void UDivineBeastsLoginScreen::HandleCredentialTextChanged(const FText&)
{
    RefreshLoginPresentation();
}

void UDivineBeastsLoginScreen::HandleViewStateChanged(int32, int32)
{
    RefreshLoginPresentation();
}

void UDivineBeastsLoginScreen::HandleCommandCompleted(
    FGuid RequestId,
    FName)
{
    if (bSubmittingLogin)
    {
        // 命令端口允许同步拒绝；记录该事实，避免Login返回后把已完成请求重新标为忙碌。
        bLoginCompletedDuringSubmit = true;
    }
    if (!ActiveLoginRequestId.IsValid() ||
        RequestId == ActiveLoginRequestId)
    {
        ActiveLoginRequestId.Invalidate();
    }
    RefreshLoginPresentation();
}

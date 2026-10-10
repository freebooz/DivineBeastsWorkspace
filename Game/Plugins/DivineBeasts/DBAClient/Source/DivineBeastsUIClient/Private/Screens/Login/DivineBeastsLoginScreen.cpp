#include "Screens/Login/DivineBeastsLoginScreen.h"
#include "Styling/DivineBeastsUIStyleBindings.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Localization/DivineBeastsUILocalization.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformMisc.h"
#include "ViewModels/Login/DivineBeastsLoginViewModel.h"

// 默认主题语义只保存在项目层CDO：当项目主题ID为空时平台兼容绑定保持旧Widget样式。
// 不修改已有Widget树、命名控件、焦点、登录输入/命令或加载任何项目纹理。
UDivineBeastsLoginScreen::UDivineBeastsLoginScreen()
{
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("LoginButton"), DivineBeasts::UI::Styling::Keys::ButtonPrimary, EGamePlatformUIStyleKind::Button));
    // 旧UMG UButton不管理内部文本样式；命名子TextBlock按相同正文视觉单独可选适配。
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("LoginButtonLabel"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text, true));
}

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
#if !UE_BUILD_SHIPPING
    // 仅显式开发回归启动允许自动提交一次；仍走真实认证命令和后端校验。
    bDevelopmentAutoLoginRequested = FParse::Param(FCommandLine::Get(), TEXT("DBADevAutoLogin"));
#endif
    ApplyDevelopmentCredentialDefaults();
    RefreshLoginPresentation();
}

void UDivineBeastsLoginScreen::ApplyDevelopmentCredentialDefaults()
{
#if !UE_BUILD_SHIPPING
    if (!AccountInput || !PasswordInput)
    {
        return;
    }

    // 只接受显式开发运行参数；不会把测试密码写进Widget资产、Config、日志或ViewState。
    FString DevelopmentUser;
    FString DevelopmentSecret;
    const bool bHasUser = FParse::Value(
        FCommandLine::Get(),
        TEXT("DBADevLoginUser="),
        DevelopmentUser);
    // 命令行会被UE记录，密码改从仅本进程继承的环境读取，禁止出现在启动参数。
    DevelopmentSecret = FPlatformMisc::GetEnvironmentVariable(TEXT("DBA_DEV_LOGIN_SECRET"));
    const bool bHasSecret = !DevelopmentSecret.IsEmpty();

    if (bHasUser && AccountInput->GetText().IsEmpty())
    {
        AccountInput->SetText(FText::FromString(DevelopmentUser));
    }
    if (bHasSecret && PasswordInput->GetText().IsEmpty())
    {
        PasswordInput->SetText(FText::FromString(DevelopmentSecret));
    }

    // 尽快释放本地临时字符串；Widget中密码仍按现有提交/离页逻辑立即清空。
    DevelopmentSecret.Reset();
#endif
}

void UDivineBeastsLoginScreen::NativeOnDeactivated()
{
    // 先解绑本页面事件再结束ViewModel页面生命周期，避免退出过程中产生无意义刷新。
    UnbindLoginEvents();
    ActiveLoginRequestId.Invalidate();
    ClearSensitiveInput();
    bDevelopmentAutoLoginRequested = false;
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
        FText Message = GetPageLoadError();
        if (Message.IsEmpty()) { Message = State ? State->ErrorText : FText::GetEmpty(); }
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
#if !UE_BUILD_SHIPPING
    // 先消费标志再提交，防止密码清空事件或同步完成回调再次进入此分支。
    if (bDevelopmentAutoLoginRequested && bCanSubmit)
    {
        bDevelopmentAutoLoginRequested = false;
        HandleLoginClicked();
    }
#endif
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

// 第三层客户端登录适配：控件瞬时持有凭据，事件提交ViewModel命令；不发HTTP、不持久化密码或显示偏好。
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
#include "Development/DivineBeastsPluginDemoLauncher.h"
#include "Development/DivineBeastsDevelopmentLoginAccounts.h"
#include "Engine/GameInstance.h"

// 默认主题语义只保存在项目层CDO：当项目主题ID为空时平台兼容绑定保持旧Widget样式。
// 不修改已有Widget树、命名控件、焦点、登录输入/命令或加载任何项目纹理。
UDivineBeastsLoginScreen::UDivineBeastsLoginScreen()
{
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("LoginButton"), DivineBeasts::UI::Styling::Keys::ButtonPrimary, EGamePlatformUIStyleKind::Button));
    // 旧UMG UButton不管理内部文本样式；命名子TextBlock按相同正文视觉单独可选适配。
    ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
        TEXT("LoginButtonLabel"), DivineBeasts::UI::Styling::Keys::TextBody, EGamePlatformUIStyleKind::Text, true));
    // 十个开发入口复用项目共享主题；资产保存按钮布局，运行时主题事件负责统一换肤。
    for (int32 Index = 1; Index <= 10; ++Index)
    {
        ThemeBindings.Add(DivineBeasts::UI::Styling::MakeBinding(
            *FString::Printf(TEXT("DevelopmentAccount%02dButton"), Index),
            DivineBeasts::UI::Styling::Keys::ButtonSecondary, EGamePlatformUIStyleKind::Button, true));
    }
}

UDivineBeastsLoginViewModel*
UDivineBeastsLoginScreen::GetLoginViewModel() const
{
    // 页面不缓存第二份 ViewModel 指针，避免状态所有权重复。
    return Cast<UDivineBeastsLoginViewModel>(GetViewModel());
}

void UDivineBeastsLoginScreen::NativeOnActivated()
{
    ++LoginActivationGeneration;
    const uint64 ActivatedGeneration = LoginActivationGeneration;
    Super::NativeOnActivated();
    // 父类的同步页面/资源通知允许关闭或重新打开页面，旧激活栈不得在失活后补绑事件。
    if (!IsActivated() || LoginActivationGeneration != ActivatedGeneration) { return; }
    InitializeDevelopmentAccounts();
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
    ++LoginActivationGeneration;
    // 先解绑本页面事件再结束ViewModel页面生命周期，避免退出过程中产生无意义刷新。
    UnbindLoginEvents();
    ActiveLoginRequestId.Invalidate();
    ClearSensitiveInput();
    bDevelopmentAutoLoginRequested = false;
    bSubmittingLogin = false;
    bLoginCompletedDuringSubmit = false;
    DevelopmentAccountNames.Reset();
    DevelopmentAccountError = FText::GetEmpty();
    RefreshDevelopmentAccounts();
    Super::NativeOnDeactivated();
}

void UDivineBeastsLoginScreen::BindLoginEvents()
{
    if (bLoginEventsBound)
    {
        return;
    }
    BindDevelopmentAccountEvents(true);

    if (LoginButton)
    {
        LoginButton->OnClicked.AddUniqueDynamic(
            this,
            &UDivineBeastsLoginScreen::HandleLoginClicked);
    }
    if (UButton* DevelopmentButton = Cast<UButton>(GetWidgetFromName(TEXT("PluginDevelopmentButton"))))
    { DevelopmentButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePluginDevelopmentClicked); }
    if (UGameInstance* Instance = GetGameInstance())
    {
        if (auto* Launcher = Instance->GetSubsystem<UDivineBeastsPluginDemoLauncher>())
        { PluginDevelopmentStatusHandle = Launcher->OnStatusChanged.AddUObject(this, &ThisClass::RefreshLoginPresentation); }
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
    if (UButton* PasswordRevealButton = GetPasswordRevealButton())
    {
        PasswordRevealButton->OnPressed.AddUniqueDynamic(this, &ThisClass::HandlePasswordRevealPressed);
        PasswordRevealButton->OnReleased.AddUniqueDynamic(this, &ThisClass::HandlePasswordRevealReleased);
        PasswordRevealButton->OnUnhovered.AddUniqueDynamic(this, &ThisClass::HandlePasswordRevealReleased);
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
    BindDevelopmentAccountEvents(false);

    if (UButton* DevelopmentButton = Cast<UButton>(GetWidgetFromName(TEXT("PluginDevelopmentButton"))))
    { DevelopmentButton->OnClicked.RemoveDynamic(this, &ThisClass::HandlePluginDevelopmentClicked); }
    if (UGameInstance* Instance = GetGameInstance())
    {
        if (auto* Launcher = Instance->GetSubsystem<UDivineBeastsPluginDemoLauncher>())
        { Launcher->OnStatusChanged.Remove(PluginDevelopmentStatusHandle); }
    }
    PluginDevelopmentStatusHandle.Reset();

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
    if (UButton* PasswordRevealButton = GetPasswordRevealButton())
    {
        PasswordRevealButton->OnPressed.RemoveDynamic(this, &ThisClass::HandlePasswordRevealPressed);
        PasswordRevealButton->OnReleased.RemoveDynamic(this, &ThisClass::HandlePasswordRevealReleased);
        PasswordRevealButton->OnUnhovered.RemoveDynamic(this, &ThisClass::HandlePasswordRevealReleased);
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
    RefreshPluginDevelopmentEntry();
    RefreshDevelopmentAccounts();
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
    if (UButton* PasswordRevealButton = GetPasswordRevealButton())
    {
        PasswordRevealButton->SetIsEnabled(IsActivated() && !bBusy && PasswordInput && !PasswordInput->GetText().IsEmpty());
        // 同步提交/忙碌事件也必须关闭正在按住的揭示，不能依赖鼠标松开一定送达。
        if (bBusy || !PasswordInput || PasswordInput->GetText().IsEmpty()) { HandlePasswordRevealReleased(); }
    }
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
        if (Message.IsEmpty()) { Message = DevelopmentAccountError; }
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

void UDivineBeastsLoginScreen::RefreshPluginDevelopmentEntry()
{
    auto* Launcher = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDivineBeastsPluginDemoLauncher>() : nullptr;
    const bool bAvailable = Launcher && Launcher->IsEntryAvailable();
    const auto* Model = GetLoginViewModel();
    const bool bBusy = ActiveLoginRequestId.IsValid() || bSubmittingLogin || (Model && Model->GetStateRef().bBusy);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("PluginDevelopmentButton"))))
    {
        Button->SetVisibility(bAvailable ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
        Button->SetIsEnabled(IsActivated() && bAvailable && !bBusy && !Launcher->IsBusy());
        // 仅入口实际可交互时插入Tab环，生产/忙碌状态保持原登录焦点顺序。
        const bool bCanNavigateToDevelopment = Button->GetIsEnabled();
        if (LoginButton && AccountInput)
        {
            LoginButton->SetNavigationRuleExplicit(EUINavigation::Next,
                bCanNavigateToDevelopment ? static_cast<UWidget*>(Button) : static_cast<UWidget*>(AccountInput));
            AccountInput->SetNavigationRuleExplicit(EUINavigation::Previous,
                bCanNavigateToDevelopment ? static_cast<UWidget*>(Button) : static_cast<UWidget*>(LoginButton));
        }
    }
    if (UTextBlock* Status = Cast<UTextBlock>(GetWidgetFromName(TEXT("PluginDevelopmentStatus"))))
    {
        Status->SetText(bAvailable ? Launcher->GetStatusText() : FText::GetEmpty());
        Status->SetVisibility(bAvailable && !Launcher->GetStatusText().IsEmpty() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
}

void UDivineBeastsLoginScreen::HandlePluginDevelopmentClicked()
{
    const auto* Model = GetLoginViewModel();
    if (!IsActivated() || bSubmittingLogin || ActiveLoginRequestId.IsValid() || (Model && Model->GetStateRef().bBusy)) { return; }
    auto* Launcher = GetGameInstance() ? GetGameInstance()->GetSubsystem<UDivineBeastsPluginDemoLauncher>() : nullptr;
    if (!Launcher || !Launcher->IsEntryAvailable() || Launcher->IsBusy()) { return; }
    // 不复制输入给子进程；退出登录意图之前取消本页自动提交并立即清空密码。
    bDevelopmentAutoLoginRequested = false;
    ClearSensitiveInput();
    Launcher->RequestLaunch();
}

UButton* UDivineBeastsLoginScreen::GetPasswordRevealButton() const
{
    // 命名可选控件不参与父类反射字段布局；查询成本仅发生在输入/激活事件中，不逐帧轮询。
    return Cast<UButton>(GetWidgetFromName(TEXT("PasswordRevealButton")));
}

void UDivineBeastsLoginScreen::ClearSensitiveInput()
{
    HandlePasswordRevealReleased();
    if (PasswordInput)
    {
        // 密码只允许作为一次命令参数存在；提交或离开页面后立即从Widget内存清空。
        PasswordInput->SetText(FText::GetEmpty());
    }
}

void UDivineBeastsLoginScreen::HandleLoginClicked()
{
    if (!AccountInput || !PasswordInput)
    {
        RefreshLoginPresentation();
        return;
    }

    const FString LoginName =
        AccountInput->GetText().ToString().TrimStartAndEnd();
    const FString Password = PasswordInput->GetText().ToString();
    SubmitCredentials(LoginName, Password);
}

void UDivineBeastsLoginScreen::SubmitCredentials(const FString& LoginName, const FString& Password)
{
    UDivineBeastsLoginViewModel* LoginViewModel = GetLoginViewModel();
    if (!IsActivated() || bSubmittingLogin || !LoginViewModel ||
        !LoginViewModel->CanSubmitCredentials() || ActiveLoginRequestId.IsValid() ||
        LoginName.IsEmpty() || Password.IsEmpty())
    {
        RefreshLoginPresentation();
        return;
    }

    bSubmittingLogin = true;
    DevelopmentAccountError = FText::GetEmpty();
    bLoginCompletedDuringSubmit = false;
    const uint64 SubmittedPageGeneration = LoginActivationGeneration;
    const FGuid SubmittedRequestId =
        LoginViewModel->Login(LoginName, Password);
    // Login允许同步完成并触发流程切页；旧调用栈不能覆盖重新激活页面的请求或清空新输入。
    if (!IsActivated() || LoginActivationGeneration != SubmittedPageGeneration ||
        GetLoginViewModel() != LoginViewModel) { return; }
    bSubmittingLogin = false;
    ActiveLoginRequestId =
        SubmittedRequestId.IsValid() && !bLoginCompletedDuringSubmit
            ? SubmittedRequestId
            : FGuid();
    ClearSensitiveInput();
    RefreshLoginPresentation();
}

void UDivineBeastsLoginScreen::InitializeDevelopmentAccounts()
{
    DevelopmentAccountNames.Reset(); DevelopmentAccountError = FText::GetEmpty();
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    if (FParse::Param(FCommandLine::Get(), TEXT("DBADevelopmentTestAccounts")))
    {
        FString Json = FPlatformMisc::GetEnvironmentVariable(TEXT("DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON"));
        if (!DivineBeasts::UI::DevelopmentLogin::ReadAccountNames(Json, DevelopmentAccountNames))
        { DevelopmentAccountError = FText::FromString(TEXT("测试账号配置不可用，请从开发启动器重新打开客户端。")); }
        Json.Reset();
    }
#endif
}

void UDivineBeastsLoginScreen::BindDevelopmentAccountEvents(bool bBind)
{
    using FCallback = void (ThisClass::*)();
    const FCallback Callbacks[] = {
        &ThisClass::HandleDevelopmentAccount01Clicked, &ThisClass::HandleDevelopmentAccount02Clicked,
        &ThisClass::HandleDevelopmentAccount03Clicked, &ThisClass::HandleDevelopmentAccount04Clicked,
        &ThisClass::HandleDevelopmentAccount05Clicked, &ThisClass::HandleDevelopmentAccount06Clicked,
        &ThisClass::HandleDevelopmentAccount07Clicked, &ThisClass::HandleDevelopmentAccount08Clicked,
        &ThisClass::HandleDevelopmentAccount09Clicked, &ThisClass::HandleDevelopmentAccount10Clicked};
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Callbacks); ++Index)
    {
        if (UButton* Button = Cast<UButton>(GetWidgetFromName(
            FName(*FString::Printf(TEXT("DevelopmentAccount%02dButton"), Index + 1)))))
        {
            // 手动给出UFunction名与函数指针，复用动态委托本身的唯一性和反射生命周期。
            const FName FunctionName(*FString::Printf(TEXT("HandleDevelopmentAccount%02dClicked"), Index + 1));
            if (bBind) { Button->OnClicked.__Internal_AddUniqueDynamic(this, Callbacks[Index], FunctionName); }
            else { Button->OnClicked.__Internal_RemoveDynamic(this, Callbacks[Index], FunctionName); }
        }
    }
}

void UDivineBeastsLoginScreen::RefreshDevelopmentAccounts()
{
    const auto* Model = GetLoginViewModel();
    const bool bAvailable = DevelopmentAccountNames.Num() == 10;
    const bool bCanSubmit = bAvailable && IsActivated() && !bSubmittingLogin &&
        !ActiveLoginRequestId.IsValid() && Model && Model->CanSubmitCredentials();
    if (UWidget* Panel = GetWidgetFromName(TEXT("DevelopmentTestAccountsPanel")))
    { Panel->SetVisibility(bAvailable ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed); }
    for (int32 Index = 0; Index < 10; ++Index)
    {
        if (UButton* Button = Cast<UButton>(GetWidgetFromName(
            FName(*FString::Printf(TEXT("DevelopmentAccount%02dButton"), Index + 1)))))
        { Button->SetIsEnabled(bCanSubmit); }
        if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(
            FName(*FString::Printf(TEXT("DevelopmentAccount%02dLabel"), Index + 1)))))
        { Label->SetText(bAvailable ? FText::FromString(DevelopmentAccountNames[Index]) : FText::GetEmpty()); }
    }
    // 开发容器实际存在时才接入键盘焦点环；忙碌状态由真实按钮禁用，失活不保留旧账号焦点。
    if (bAvailable && LoginButton && AccountInput)
    {
        auto* PluginButton = Cast<UButton>(GetWidgetFromName(TEXT("PluginDevelopmentButton")));
        UWidget* Previous = PluginButton && PluginButton->GetIsEnabled()
            ? static_cast<UWidget*>(PluginButton) : static_cast<UWidget*>(LoginButton);
        for (int32 Index = 0; Index < 10; ++Index)
        {
            auto* Button = Cast<UButton>(GetWidgetFromName(FName(*FString::Printf(TEXT("DevelopmentAccount%02dButton"), Index + 1))));
            if (!Button) { continue; }
            Previous->SetNavigationRuleExplicit(EUINavigation::Next, Button);
            Button->SetNavigationRuleExplicit(EUINavigation::Previous, Previous);
            Previous = Button;
        }
        Previous->SetNavigationRuleExplicit(EUINavigation::Next, AccountInput);
        AccountInput->SetNavigationRuleExplicit(EUINavigation::Previous, Previous);
    }
}

void UDivineBeastsLoginScreen::SubmitDevelopmentAccount(int32 Index)
{
#if !UE_BUILD_SHIPPING && !UE_BUILD_TEST
    const auto* Model = GetLoginViewModel();
    if (!IsActivated() || bSubmittingLogin || ActiveLoginRequestId.IsValid() || !Model ||
        !Model->CanSubmitCredentials() || !DevelopmentAccountNames.IsValidIndex(Index)) { return; }
    FString Json = FPlatformMisc::GetEnvironmentVariable(TEXT("DBA_DEV_TEST_LOGIN_ACCOUNTS_JSON"));
    FString Name, Password;
    const bool bValid = DivineBeasts::UI::DevelopmentLogin::ReadCredential(
        Json, Index, DevelopmentAccountNames[Index], Name, Password);
    Json.Reset();
    if (!bValid)
    {
        DevelopmentAccountNames.Reset();
        DevelopmentAccountError = FText::FromString(TEXT("测试账号配置已变化，请重新打开登录页。"));
        RefreshLoginPresentation(); return;
    }
    bDevelopmentAutoLoginRequested = false;
    if (AccountInput) { AccountInput->SetText(FText::FromString(Name)); }
    // 口令不填入输入框或ViewState，只作为本次真实登录命令参数存在。
    SubmitCredentials(Name, Password);
    Password.Reset();
#endif
}

void UDivineBeastsLoginScreen::HandleDevelopmentAccount01Clicked() { SubmitDevelopmentAccount(0); }
void UDivineBeastsLoginScreen::HandleDevelopmentAccount02Clicked() { SubmitDevelopmentAccount(1); }
void UDivineBeastsLoginScreen::HandleDevelopmentAccount03Clicked() { SubmitDevelopmentAccount(2); }
void UDivineBeastsLoginScreen::HandleDevelopmentAccount04Clicked() { SubmitDevelopmentAccount(3); }
void UDivineBeastsLoginScreen::HandleDevelopmentAccount05Clicked() { SubmitDevelopmentAccount(4); }
void UDivineBeastsLoginScreen::HandleDevelopmentAccount06Clicked() { SubmitDevelopmentAccount(5); }
void UDivineBeastsLoginScreen::HandleDevelopmentAccount07Clicked() { SubmitDevelopmentAccount(6); }
void UDivineBeastsLoginScreen::HandleDevelopmentAccount08Clicked() { SubmitDevelopmentAccount(7); }
void UDivineBeastsLoginScreen::HandleDevelopmentAccount09Clicked() { SubmitDevelopmentAccount(8); }
void UDivineBeastsLoginScreen::HandleDevelopmentAccount10Clicked() { SubmitDevelopmentAccount(9); }

void UDivineBeastsLoginScreen::HandleCredentialTextChanged(const FText&)
{
    RefreshLoginPresentation();
}

void UDivineBeastsLoginScreen::HandlePasswordRevealPressed()
{
    const auto* LoginViewModel = GetLoginViewModel();
    if (IsActivated() && PasswordInput && !PasswordInput->GetText().IsEmpty() &&
        !ActiveLoginRequestId.IsValid() && (!LoginViewModel || !LoginViewModel->GetStateRef().bBusy))
    { PasswordInput->SetIsPassword(false); }
}

void UDivineBeastsLoginScreen::HandlePasswordRevealReleased()
{
    if (PasswordInput) { PasswordInput->SetIsPassword(true); }
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

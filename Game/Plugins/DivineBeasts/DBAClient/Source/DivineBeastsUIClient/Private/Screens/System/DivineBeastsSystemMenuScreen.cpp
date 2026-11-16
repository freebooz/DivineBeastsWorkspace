// 项目世界游戏菜单只投影只读VM并发出命令；生命周期交给既有平台UI，退出登录复用唯一ApplicationFlow。
#include "Screens/System/DivineBeastsSystemMenuScreen.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"
#include "ViewModels/DivineBeastsUIViewModel.h"

UDivineBeastsSystemMenuScreen::UDivineBeastsSystemMenuScreen() { UIDomain = EDivineBeastsUIDomain::System; }

void UDivineBeastsSystemMenuScreen::NativeOnActivated()
{
    if (auto* Label = Cast<UTextBlock>(LogoutButton->GetChildAt(0))) DefaultLogoutLabel = Label->GetText();
    Super::NativeOnActivated();
    if (!IsActivated()) return;
    ResumeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResume);
    SettingsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettings);
    LogoutButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLogout);
    QuitButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuit);
    if (auto* Player = GetOwningLocalPlayer())
    {
        MenuService = Player->GetSubsystem<UDivineBeastsUIClientSubsystem>();
        if (auto* Service = MenuService.Get())
            WorldMenuActionHandle = Service->OnWorldMenuActionChanged.AddUObject(this, &ThisClass::RefreshCommands);
    }
    RefreshCommands();
}
void UDivineBeastsSystemMenuScreen::NativeOnDeactivated()
{
    if (auto* Service = MenuService.Get()) Service->OnWorldMenuActionChanged.Remove(WorldMenuActionHandle);
    MenuService.Reset();
    WorldMenuActionHandle.Reset();
    if (auto* Label = Cast<UTextBlock>(LogoutButton->GetChildAt(0))) Label->SetText(DefaultLogoutLabel);
    ResumeButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleResume);
    SettingsButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleSettings);
    LogoutButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleLogout);
    QuitButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleQuit);
    Super::NativeOnDeactivated();
}
void UDivineBeastsSystemMenuScreen::OnViewModelStateChanged(int32 Revision, int32 PageGeneration)
{
    Super::OnViewModelStateChanged(Revision, PageGeneration);
    if (IsActivated()) RefreshCommands();
}
void UDivineBeastsSystemMenuScreen::RefreshCommands()
{
    auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel());
    const auto* Service = MenuService.Get();
    const FDivineBeastsWorldMenuAction Action = Service ? Service->GetWorldMenuAction() : FDivineBeastsWorldMenuAction{};
    const bool bWorldAction = !Action.Label.IsEmpty();
    LogoutButton->SetIsEnabled(bWorldAction ? Action.bEnabled :
        VM && VM->GetStateRef().AllowedCommands.Contains(TEXT("Logout")) && !VM->GetStateRef().bBusy);
    if (auto* Label = Cast<UTextBlock>(LogoutButton->GetChildAt(0)))
        Label->SetText(bWorldAction ? Action.Label : DefaultLogoutLabel);
    FText Error = GetPageLoadError();
    if (Error.IsEmpty() && bWorldAction) Error = Action.Status;
    if (Error.IsEmpty() && VM && !VM->GetLastCommandErrorCode().IsNone()) Error = FText::FromString(TEXT("命令未完成，请稍后重试。"));
    ErrorText->SetText(Error);
}
void UDivineBeastsSystemMenuScreen::HandleResume()
{
    if (auto* LP = GetOwningLocalPlayer()) if (auto* UI = LP->GetSubsystem<UDivineBeastsUIClientSubsystem>()) UI->CloseSystemMenu();
}
void UDivineBeastsSystemMenuScreen::HandleSettings()
{
    if (auto* LP = GetOwningLocalPlayer()) if (auto* UI = LP->GetSubsystem<UDivineBeastsUIClientSubsystem>()) UI->OpenSystemSettings();
}
void UDivineBeastsSystemMenuScreen::HandleLogout()
{
    if (auto* Service = MenuService.Get())
    {
        const auto Action = Service->GetWorldMenuAction();
        if (!Action.Label.IsEmpty())
        {
            if (Action.bEnabled) Action.Execute.ExecuteIfBound();
            return;
        }
    }
    // 注销成功/忙碌快照会自动关闭菜单；不能先关页取消正在提交的真实注销命令。
    if (auto* VM = Cast<UDivineBeastsUIViewModel>(GetViewModel())) VM->Logout();
}
void UDivineBeastsSystemMenuScreen::HandleQuit()
{
    HandleResume();
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}
bool UDivineBeastsSystemMenuScreen::NativeOnHandleBackAction() { HandleResume(); return true; }
FReply UDivineBeastsSystemMenuScreen::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    // UIOnly下世界输入不会收到ESC；原生事件消费一次，防CommonUI Back重复开关。
    if (Event.GetKey() == EKeys::Escape) { HandleResume(); return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

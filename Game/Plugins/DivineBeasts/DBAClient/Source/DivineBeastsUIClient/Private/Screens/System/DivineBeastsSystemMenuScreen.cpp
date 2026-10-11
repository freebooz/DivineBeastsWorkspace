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
    Super::NativeOnActivated();
    if (!IsActivated()) return;
    ResumeButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResume);
    SettingsButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettings);
    LogoutButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLogout);
    QuitButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuit);
    RefreshCommands();
}
void UDivineBeastsSystemMenuScreen::NativeOnDeactivated()
{
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
    LogoutButton->SetIsEnabled(VM && VM->GetStateRef().AllowedCommands.Contains(TEXT("Logout")) && !VM->GetStateRef().bBusy);
    FText Error = GetPageLoadError();
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

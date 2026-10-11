// Monolith负责视觉布局，页面只显示事件快照并提交候选设备命令；不持有设备持久状态或联网业务。
#include "Screens/System/DivineBeastsDeviceSettingsScreen.h"
#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Engine/LocalPlayer.h"
#include "InputCoreTypes.h"
#include "ViewModels/System/DivineBeastsSystemSettingsViewModel.h"

UDivineBeastsDeviceSettingsScreen::UDivineBeastsDeviceSettingsScreen() { UIDomain = EDivineBeastsUIDomain::System; }

UDivineBeastsSystemSettingsViewModel* UDivineBeastsDeviceSettingsScreen::SettingsVM() const { return Cast<UDivineBeastsSystemSettingsViewModel>(GetViewModel()); }
void UDivineBeastsDeviceSettingsScreen::NativeOnActivated()
{
    PresentedDeviceRevision = -1;
    Super::NativeOnActivated();
    if (!IsActivated()) return;
    ApplyButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleApply);
    ConfirmButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirm);
    RevertButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRevert);
    BackButton->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleBack);
    RefreshPresentation();
}
void UDivineBeastsDeviceSettingsScreen::NativeOnDeactivated()
{
    ApplyButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleApply);
    ConfirmButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleConfirm);
    RevertButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleRevert);
    BackButton->OnClicked.RemoveDynamic(this, &ThisClass::HandleBack);
    Super::NativeOnDeactivated(); // 平台EndPage使VM解绑并回滚本页未确认预览。
}
void UDivineBeastsDeviceSettingsScreen::OnViewModelStateChanged(int32 Revision, int32 PageGeneration)
{
    Super::OnViewModelStateChanged(Revision, PageGeneration);
    if (IsActivated()) RefreshPresentation();
}
void UDivineBeastsDeviceSettingsScreen::RefreshPresentation()
{
    auto* VM = SettingsVM();
    if (!VM) return;
    const auto& Snapshot = VM->DeviceSnapshot;
    if (PresentedDeviceRevision != Snapshot.Revision)
    {
        PresentedDeviceRevision = Snapshot.Revision;
        ResolutionCombo->ClearOptions();
        for (const auto& Size : VM->SupportedResolutions) ResolutionCombo->AddOption(FString::Printf(TEXT("%d × %d"), Size.X, Size.Y));
        ResolutionCombo->SetSelectedIndex(VM->SupportedResolutions.IndexOfByKey(Snapshot.Current.Resolution));
        WindowModeCombo->ClearOptions();
        for (const TCHAR* Mode : {TEXT("窗口"), TEXT("无边框全屏"), TEXT("全屏")}) WindowModeCombo->AddOption(Mode);
        WindowModeCombo->SetSelectedIndex(static_cast<int32>(Snapshot.Current.WindowMode));
        QualityCombo->ClearOptions();
        for (const TCHAR* Level : {TEXT("低"), TEXT("中"), TEXT("高"), TEXT("极高"), TEXT("电影")}) QualityCombo->AddOption(Level);
        const auto& Q = Snapshot.Current;
        const bool bUniformQuality = Q.ViewDistanceQuality == Q.AntiAliasingQuality && Q.ViewDistanceQuality == Q.ShadowQuality &&
            Q.ViewDistanceQuality == Q.GlobalIlluminationQuality && Q.ViewDistanceQuality == Q.ReflectionQuality && Q.ViewDistanceQuality == Q.PostProcessQuality &&
            Q.ViewDistanceQuality == Q.TextureQuality && Q.ViewDistanceQuality == Q.EffectsQuality && Q.ViewDistanceQuality == Q.FoliageQuality && Q.ViewDistanceQuality == Q.ShadingQuality;
        QualityCombo->AddOption(TEXT("自定义（保留当前）"));
        QualityCombo->SetSelectedIndex(bUniformQuality ? Q.ViewDistanceQuality : 5);
    }
    const bool bCanEdit = VM->bDeviceSettingsAvailable && !Snapshot.bPreviewActive;
    ResolutionCombo->SetIsEnabled(bCanEdit);
    WindowModeCombo->SetIsEnabled(bCanEdit);
    QualityCombo->SetIsEnabled(bCanEdit);
    ApplyButton->SetIsEnabled(bCanEdit && !VM->SupportedResolutions.IsEmpty());
    ConfirmButton->SetIsEnabled(Snapshot.bPreviewActive);
    RevertButton->SetIsEnabled(Snapshot.bPreviewActive);
    StatusText->SetText(FText::FromString(Snapshot.bPreviewActive ? TEXT("请在15秒内确认；超时或返回将恢复原设置。") : TEXT("调整显示与画质，然后点击应用。")));
    ErrorText->SetText(VM->DeviceError);
}
void UDivineBeastsDeviceSettingsScreen::HandleApply()
{
    auto* VM = SettingsVM();
    if (!VM || !VM->SupportedResolutions.IsValidIndex(ResolutionCombo->GetSelectedIndex())) return;
    auto Candidate = VM->DeviceSnapshot.Current;
    Candidate.Resolution = VM->SupportedResolutions[ResolutionCombo->GetSelectedIndex()];
    Candidate.WindowMode = static_cast<EGamePlatformWindowMode>(WindowModeCombo->GetSelectedIndex());
    const int32 Quality = QualityCombo->GetSelectedIndex();
    if (Quality >= 0 && Quality <= 4)
    {
        Candidate.ViewDistanceQuality = Candidate.AntiAliasingQuality = Candidate.ShadowQuality = Candidate.GlobalIlluminationQuality =
            Candidate.ReflectionQuality = Candidate.PostProcessQuality = Candidate.TextureQuality = Candidate.EffectsQuality = Candidate.FoliageQuality = Candidate.ShadingQuality = Quality;
    }
    VM->PreviewDeviceSettings(Candidate);
}
void UDivineBeastsDeviceSettingsScreen::HandleConfirm() { if (auto* VM = SettingsVM()) VM->ConfirmDeviceSettings(); }
void UDivineBeastsDeviceSettingsScreen::HandleRevert() { if (auto* VM = SettingsVM()) VM->RevertDeviceSettings(); }
void UDivineBeastsDeviceSettingsScreen::HandleBack()
{
    if (auto* LP = GetOwningLocalPlayer()) if (auto* UI = LP->GetSubsystem<UDivineBeastsUIClientSubsystem>()) UI->CloseSystemSettings();
}
bool UDivineBeastsDeviceSettingsScreen::NativeOnHandleBackAction() { HandleBack(); return true; }
FReply UDivineBeastsDeviceSettingsScreen::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Event.GetKey() == EKeys::Escape) { HandleBack(); return FReply::Handled(); }
    return Super::NativeOnPreviewKeyDown(Geometry, Event);
}

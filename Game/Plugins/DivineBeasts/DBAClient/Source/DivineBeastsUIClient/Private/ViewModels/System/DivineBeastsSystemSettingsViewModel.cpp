// 项目设置ViewModel只调用平台设备接口；保存、参数校验和预览原值均由GamePlatformSettings唯一拥有。
#include "ViewModels/System/DivineBeastsSystemSettingsViewModel.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Interfaces/IGamePlatformDeviceSettingsService.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformTime.h"

IGamePlatformDeviceSettingsService* UDivineBeastsSystemSettingsViewModel::GetDeviceService() const
{
    auto* GI = SettingsGameInstance.Get();
    return GI ? IGamePlatformDeviceSettingsService::Get(*GI) : nullptr;
}

void UDivineBeastsSystemSettingsViewModel::OnPageBegan()
{
    Super::OnPageBegan();
    auto* UIOwner = GetTypedOuter<UDivineBeastsUIClientSubsystem>();
    SettingsGameInstance = UIOwner && UIOwner->GetLocalPlayer() ? UIOwner->GetLocalPlayer()->GetGameInstance() : nullptr;
    auto* Service = GetDeviceService();
    bDeviceSettingsAvailable = Service != nullptr;
    DeviceError = Service ? FText::GetEmpty() : FText::FromString(TEXT("当前环境暂不支持修改设备设置。"));
    SupportedResolutions.Reset();
    UKismetSystemLibrary::GetSupportedFullscreenResolutions(SupportedResolutions);
    if (Service)
    {
        DeviceSnapshot = Service->GetSnapshot();
        SupportedResolutions.AddUnique(DeviceSnapshot.Current.Resolution);
        const int32 Generation = GetPageGeneration();
        FGamePlatformResult Result;
        const auto Installing = MakeShared<bool>(true);
        const auto Subscription = Service->Subscribe(this, [WeakThis = TWeakObjectPtr<ThisClass>(this), Generation, Installing](const auto& Snapshot)
        {
            if (auto* VM = WeakThis.Get(); VM && VM->IsPageActive() && VM->GetPageGeneration() == Generation)
            { VM->DeviceSnapshot = Snapshot; if (!*Installing) VM->MarkStateChanged(); }
        }, Result);
        *Installing = false;
        // 初始订阅会同步通知，观察者可以关闭页面，不能保留已失活页面的迟到句柄。
        if (IsPageActive() && GetPageGeneration() == Generation) DeviceSubscription = Subscription;
        else Service->Unsubscribe(Subscription);
        if (!Result.IsSuccess()) { bDeviceSettingsAvailable = false; DeviceError = FText::FromString(Result.Message); }
    }
    if (IsPageActive()) MarkStateChanged();
}

void UDivineBeastsSystemSettingsViewModel::OnPageEnded()
{
    if (auto* Service = GetDeviceService()) Service->Unsubscribe(DeviceSubscription);
    DeviceSubscription = {};
    if (bCommandInProgress) bCleanupAfterCommand = true;
    else CleanupDeviceSettings();
    Super::OnPageEnded();
}

bool UDivineBeastsSystemSettingsViewModel::PublishResult(const FGamePlatformResult& Result)
{
    DeviceError = Result.IsSuccess() ? FText::GetEmpty() : FText::FromString(Result.Message);
    if (IsPageActive()) MarkStateChanged();
    return Result.IsSuccess();
}

bool UDivineBeastsSystemSettingsViewModel::PreviewDeviceSettings(FGamePlatformDeviceSettings Candidate)
{
    auto* Service = GetDeviceService();
    if (!IsPageActive() || !Service || bCommandInProgress || bOwnsPreview) return false;
    bCommandInProgress = true;
    auto Result = Service->StageDeviceSettings(Candidate);
    bOwnsStagedSettings = Result.IsSuccess();
    if (Result.IsSuccess() && IsPageActive())
    {
        // Apply会同步发布快照，先记录所有权，使通知期间失活能在返回后撤销本页预览。
        bOwnsPreview = true;
        Result = Service->ApplyStagedSettings(EGamePlatformDeviceSettingsApplyMode::Preview);
        if (!Result.IsSuccess()) bOwnsPreview = false;
    }
    bCommandInProgress = false;
    if (bCleanupAfterCommand || !IsPageActive()) { CleanupDeviceSettings(); return false; }
    if (!Result.IsSuccess() && bOwnsStagedSettings) CleanupDeviceSettings();
    if (bOwnsPreview)
    {
        // 异常显示模式必须按真实时间恢复；世界暂停/时间缩放不能延长15秒安全期限。
        FTSTicker::RemoveTicker(PreviewTimeout);
        PreviewDeadlineSeconds = FPlatformTime::Seconds() + 15.0;
        const int32 Generation = GetPageGeneration();
        PreviewTimeout = FTSTicker::GetCoreTicker().AddTicker(TEXT("DBA.SettingsPreviewTimeout"), 0.1f,
            [WeakThis = TWeakObjectPtr<ThisClass>(this), Generation](float)
            {
                auto* VM = WeakThis.Get();
                if (!VM || !VM->IsPageActive() || VM->GetPageGeneration() != Generation || !VM->bOwnsPreview) return false;
                if (FPlatformTime::Seconds() >= VM->PreviewDeadlineSeconds && !VM->bCommandInProgress)
                    VM->RevertDeviceSettings();
                // 失败仍保留本页预览资格并在后续调度重试；不会把失败冒充已回滚或复活旧页面。
                return VM->IsPageActive() && VM->GetPageGeneration() == Generation && VM->bOwnsPreview;
            });
    }
    return PublishResult(Result);
}

bool UDivineBeastsSystemSettingsViewModel::ConfirmDeviceSettings()
{
    auto* Service = GetDeviceService();
    if (!IsPageActive() || !Service || !bOwnsPreview || bCommandInProgress) return false;
    bCommandInProgress = true;
    const auto Result = Service->ConfirmPreview();
    if (Result.IsSuccess()) bOwnsPreview = false;
    if (Result.IsSuccess()) bOwnsStagedSettings = false;
    bCommandInProgress = false;
    if (bCleanupAfterCommand || !IsPageActive()) { CleanupDeviceSettings(); return false; }
    if (!bOwnsPreview) { FTSTicker::RemoveTicker(PreviewTimeout); PreviewTimeout.Reset(); PreviewDeadlineSeconds = 0.0; }
    return PublishResult(Result);
}

bool UDivineBeastsSystemSettingsViewModel::RevertDeviceSettings()
{
    auto* Service = GetDeviceService();
    if (!Service || !bOwnsPreview || bCommandInProgress) return false;
    bCommandInProgress = true;
    const auto Result = Service->CancelPreview();
    if (Result.IsSuccess()) bOwnsPreview = false;
    if (Result.IsSuccess()) bOwnsStagedSettings = false;
    bCommandInProgress = false;
    if (bCleanupAfterCommand || !IsPageActive()) { CleanupDeviceSettings(); return Result.IsSuccess(); }
    if (!bOwnsPreview) { FTSTicker::RemoveTicker(PreviewTimeout); PreviewTimeout.Reset(); PreviewDeadlineSeconds = 0.0; }
    return PublishResult(Result);
}

void UDivineBeastsSystemSettingsViewModel::CleanupDeviceSettings()
{
    bCleanupAfterCommand = false;
    FTSTicker::RemoveTicker(PreviewTimeout);
    PreviewTimeout.Reset();
    PreviewDeadlineSeconds = 0.0;
    auto* Service = GetDeviceService();
    if (Service && bOwnsPreview)
    {
        const auto Result = Service->CancelPreview();
        if (!Result.IsSuccess()) UE_LOG(LogTemp, Warning, TEXT("[DBA UI] Settings preview cleanup failed: %s"), *Result.Code.ToString());
    }
    else if (Service && bOwnsStagedSettings)
    {
        // Stage成功但页面在通知期间退出/Apply失败时，只丢弃本页候选，不留下脏设置。
        const auto Result = Service->DiscardStagedSettings();
        if (!Result.IsSuccess()) UE_LOG(LogTemp, Warning, TEXT("[DBA UI] Staged settings cleanup failed: %s"), *Result.Code.ToString());
    }
    bOwnsPreview = false;
    bOwnsStagedSettings = false;
}

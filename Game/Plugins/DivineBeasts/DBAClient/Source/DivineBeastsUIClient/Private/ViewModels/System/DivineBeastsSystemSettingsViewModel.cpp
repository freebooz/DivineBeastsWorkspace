// 项目设置ViewModel消费平台设备与SFX接口；各领域唯一拥有持久值，UI只投影并提交命令。
#include "ViewModels/System/DivineBeastsSystemSettingsViewModel.h"
#include "DivineBeastsUIClientSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Interfaces/IGamePlatformDeviceSettingsService.h"
#include "Interfaces/IGamePlatformSFXUserSettingsService.h"
#include "Kismet/KismetSystemLibrary.h"
#include "HAL/PlatformTime.h"

IGamePlatformDeviceSettingsService* UDivineBeastsSystemSettingsViewModel::GetDeviceService() const
{
    auto* GI = SettingsGameInstance.Get();
    return GI ? IGamePlatformDeviceSettingsService::Get(*GI) : nullptr;
}
IGamePlatformSFXUserSettingsService* UDivineBeastsSystemSettingsViewModel::GetAudioService() const
{
    auto* GI = SettingsGameInstance.Get();
    return GI ? IGamePlatformSFXUserSettingsService::Get(*GI) : nullptr;
}
void UDivineBeastsSystemSettingsViewModel::HandleAudioSettingsChanged(const FGamePlatformSFXUserVolumes& Snapshot, int32 ExpectedPageGeneration)
{
    if (!IsPageActive() || GetPageGeneration() != ExpectedPageGeneration) return;
    AudioSnapshot = Snapshot;
    MarkStateChanged();
}

void UDivineBeastsSystemSettingsViewModel::OnPageBegan()
{
    Super::OnPageBegan();
    // BeginPage重复进入也不叠加音频订阅；解绑旧来源后再安装新页面代次。
    if (auto* PreviousAudio = GetAudioService()) PreviousAudio->RemoveChangedHandler(AudioSubscription);
    AudioSubscription.Reset();
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
    auto* Audio = GetAudioService();
    bAudioSettingsAvailable = Audio != nullptr;
    AudioSnapshot = {};
    AudioError = Audio ? FText::GetEmpty() : FText::FromString(TEXT("当前环境暂不支持修改音量。"));
    if (Audio && IsPageActive())
    {
        AudioSnapshot = Audio->GetSnapshot();
        const int32 ExpectedGeneration = GetPageGeneration();
        const auto ExpectedGI = SettingsGameInstance;
        AudioSubscription = Audio->AddChangedHandler(FGamePlatformSFXUserVolumesChanged::FDelegate::CreateWeakLambda(this,
            [this, ExpectedGeneration, ExpectedGI](const auto& Snapshot)
            {
                if (SettingsGameInstance == ExpectedGI) HandleAudioSettingsChanged(Snapshot, ExpectedGeneration);
            }));
    }
    if (IsPageActive()) MarkStateChanged();
}

void UDivineBeastsSystemSettingsViewModel::OnPageEnded()
{
    if (auto* Audio = GetAudioService())
    {
        Audio->RemoveChangedHandler(AudioSubscription);
        // 键盘调整未经历鼠标CaptureEnd时也保存；音量独立于需要回滚的显示预览。
        const auto Result = Audio->CommitVolumes();
        if (!Result.IsSuccess()) UE_LOG(LogTemp, Warning, TEXT("[DBA UI] Audio settings save failed: %s"), *Result.Code.ToString());
    }
    AudioSubscription.Reset();
    bAudioSettingsAvailable = false;
    if (auto* Service = GetDeviceService()) Service->Unsubscribe(DeviceSubscription);
    DeviceSubscription = {};
    if (bCommandInProgress) bCleanupAfterCommand = true;
    else CleanupDeviceSettings();
    Super::OnPageEnded();
}

bool UDivineBeastsSystemSettingsViewModel::SetAudioVolume(EGamePlatformSFXSoundGroup Group, float Value)
{
    auto* Service = GetAudioService();
    if (!IsPageActive() || !Service) return false;
    const int32 CommandGeneration = GetPageGeneration();
    const auto Result = Service->SetVolume(Group, Value);
    if (!IsPageActive() || GetPageGeneration() != CommandGeneration) return false;
    AudioError = Result.IsSuccess() ? FText::GetEmpty() : FText::FromString(Result.Message);
    if (IsPageActive()) MarkStateChanged();
    return Result.IsSuccess();
}
bool UDivineBeastsSystemSettingsViewModel::SaveAudioVolumes()
{
    auto* Service = GetAudioService();
    if (!IsPageActive() || !Service) return false;
    const int32 CommandGeneration = GetPageGeneration();
    const auto Result = Service->CommitVolumes();
    if (!IsPageActive() || GetPageGeneration() != CommandGeneration) return false;
    AudioError = Result.IsSuccess() ? FText::GetEmpty() : FText::FromString(Result.Message);
    if (IsPageActive()) MarkStateChanged();
    return Result.IsSuccess();
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
        // 设备预览以单调真实时钟计15秒：只在截止点触发一次，不以0.1秒Ticker轮询UI/服务。
        // 失败时单次重试仍由本页拥有；世界Pause/时间缩放不影响安全回滚。
        PreviewDeadlineSeconds = FPlatformTime::Seconds() + 15.0;
        SchedulePreviewDeadline(GetPageGeneration(), 15.0);
    }
    return PublishResult(Result);
}

void UDivineBeastsSystemSettingsViewModel::SchedulePreviewDeadline(
    int32 ExpectedPageGeneration, double DelaySeconds)
{
    FTSTicker::RemoveTicker(PreviewTimeout);
    PreviewTimeout.Reset();
    if (!IsPageActive() || !bOwnsPreview ||
        GetPageGeneration() != ExpectedPageGeneration ||
        PreviewDeadlineSeconds <= 0.0)
    {
        return;
    }
    // CoreTicker支持真实时间延时调度，回调只执行一次；不依赖World或每帧轮询。
    const float Wait = static_cast<float>(FMath::Clamp(DelaySeconds, 0.01, 15.0));
    PreviewTimeout = FTSTicker::GetCoreTicker().AddTicker(
        TEXT("DBA.SettingsPreviewDeadline"), Wait,
        [WeakThis = TWeakObjectPtr<ThisClass>(this), ExpectedPageGeneration](float)
        {
            if (auto* VM = WeakThis.Get())
            {
                // 先解除本次句柄，随后回滚/重入取消不会误移除未来重试。
                VM->PreviewTimeout.Reset();
                VM->HandlePreviewDeadline(ExpectedPageGeneration);
            }
            return false; // 一次性到期事件，不创建周期Ticker。
        });
}

void UDivineBeastsSystemSettingsViewModel::HandlePreviewDeadline(int32 ExpectedPageGeneration)
{
    if (!IsPageActive() || !bOwnsPreview ||
        GetPageGeneration() != ExpectedPageGeneration) return;
    const double RemainingSeconds = PreviewDeadlineSeconds - FPlatformTime::Seconds();
    if (RemainingSeconds > 0.001)
    {
        // 极少数调度器提前触发的情况，仍等待同一个绝对截止点。
        SchedulePreviewDeadline(ExpectedPageGeneration, RemainingSeconds);
        return;
    }
    if (bCommandInProgress)
    {
        // 当前命令栈可能同步发布设置事件；短延时等待其退出，避免回滚时重入。
        SchedulePreviewDeadline(ExpectedPageGeneration, 0.25);
        return;
    }
    if (!RevertDeviceSettings() && IsPageActive() && bOwnsPreview &&
        GetPageGeneration() == ExpectedPageGeneration)
    {
        // 平台取消失败仍保持原预览资格，并在一秒后再执行一次明确的回滚动作。
        // 不将受理/失败伪装成已回滚；状态和中文失败提示来自原有PublishResult。
        SchedulePreviewDeadline(ExpectedPageGeneration, 1.0);
    }
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

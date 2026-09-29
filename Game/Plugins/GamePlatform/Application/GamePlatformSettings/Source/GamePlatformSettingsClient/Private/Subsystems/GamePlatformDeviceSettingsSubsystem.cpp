#include "Subsystems/GamePlatformDeviceSettingsSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Misc/App.h"
#include "Policy/GamePlatformDeviceSettingsPolicy.h"

namespace
{
    constexpr int32 MaxSettingsSubscriptions = 64;

    EWindowMode::Type ToNativeWindowMode(
        const EGamePlatformWindowMode Mode)
    {
        switch (Mode)
        {
        case EGamePlatformWindowMode::Windowed:
            return EWindowMode::Windowed;
        case EGamePlatformWindowMode::Fullscreen:
            return EWindowMode::Fullscreen;
        case EGamePlatformWindowMode::WindowedFullscreen:
        default:
            return EWindowMode::WindowedFullscreen;
        }
    }

    EGamePlatformWindowMode FromNativeWindowMode(
        const EWindowMode::Type Mode)
    {
        switch (Mode)
        {
        case EWindowMode::Windowed:
            return EGamePlatformWindowMode::Windowed;
        case EWindowMode::Fullscreen:
            return EGamePlatformWindowMode::Fullscreen;
        case EWindowMode::WindowedFullscreen:
        default:
            return EGamePlatformWindowMode::WindowedFullscreen;
        }
    }

    FGamePlatformDeviceSettings ReadNative(
        const UGameUserSettings& Native)
    {
        FGamePlatformDeviceSettings Result;
        Result.Resolution = Native.GetScreenResolution();
        Result.WindowMode = FromNativeWindowMode(Native.GetFullscreenMode());
        Result.bVSyncEnabled = Native.IsVSyncEnabled();
        Result.FrameRateLimit = Native.GetFrameRateLimit();
        Result.ViewDistanceQuality = Native.GetViewDistanceQuality();
        Result.AntiAliasingQuality = Native.GetAntiAliasingQuality();
        Result.ShadowQuality = Native.GetShadowQuality();
        Result.GlobalIlluminationQuality =
            Native.GetGlobalIlluminationQuality();
        Result.ReflectionQuality = Native.GetReflectionQuality();
        Result.PostProcessQuality = Native.GetPostProcessingQuality();
        Result.TextureQuality = Native.GetTextureQuality();
        Result.EffectsQuality = Native.GetVisualEffectQuality();
        Result.FoliageQuality = Native.GetFoliageQuality();
        Result.ShadingQuality = Native.GetShadingQuality();
        return Result;
    }

    void WriteNative(
        UGameUserSettings& Native,
        const FGamePlatformDeviceSettings& Settings)
    {
        Native.SetScreenResolution(Settings.Resolution);
        Native.SetFullscreenMode(ToNativeWindowMode(Settings.WindowMode));
        Native.SetVSyncEnabled(Settings.bVSyncEnabled);
        Native.SetFrameRateLimit(Settings.FrameRateLimit);
        Native.SetViewDistanceQuality(Settings.ViewDistanceQuality);
        Native.SetAntiAliasingQuality(Settings.AntiAliasingQuality);
        Native.SetShadowQuality(Settings.ShadowQuality);
        Native.SetGlobalIlluminationQuality(
            Settings.GlobalIlluminationQuality);
        Native.SetReflectionQuality(Settings.ReflectionQuality);
        Native.SetPostProcessingQuality(Settings.PostProcessQuality);
        Native.SetTextureQuality(Settings.TextureQuality);
        Native.SetVisualEffectQuality(Settings.EffectsQuality);
        Native.SetFoliageQuality(Settings.FoliageQuality);
        Native.SetShadingQuality(Settings.ShadingQuality);
    }
}

bool UGamePlatformDeviceSettingsSubsystem::ShouldCreateSubsystem(
    UObject* Outer) const
{
    return !IsRunningDedicatedServer() &&
        !IsRunningCommandlet() &&
        Cast<UGameInstance>(Outer) != nullptr &&
        Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformDeviceSettingsSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ScopeId = FGuid::NewGuid();
    Generation = 1;

    if (UGameUserSettings* Native =
        UGameUserSettings::GetGameUserSettings())
    {
        // 真实客户端环境由引擎负责版本与损坏配置恢复；插件不维护第二套配置文件。
        // 普通PIE只读取当前进程内存快照：ValidateSettings可能清理陈旧磁盘配置，不能让多PIE测试产生设备级副作用。
        if (IsMutationEnvironmentAllowed())
        {
            Native->LoadSettings(false);
            Native->ValidateSettings();
        }

        Snapshot.Current = ReadNative(*Native);
        Snapshot.Staged = Snapshot.Current;
        Snapshot.Revision = 1;
        RefreshDerivedState();
    }
}

void UGamePlatformDeviceSettingsSubsystem::Deinitialize()
{
    if (Snapshot.bPreviewActive)
    {
        // 退出期间尽力恢复预览前状态，避免未确认设置泄漏到同进程后续实例。
        FGamePlatformResult Ignored;
        if (CanMutate(Ignored))
        {
            ApplyToEngine(PreviewBaseline);
            if (UGameUserSettings* Native =
                UGameUserSettings::GetGameUserSettings())
            {
                Native->ConfirmVideoMode();
            }
        }
    }

    Subscriptions.Reset();
    Snapshot = FGamePlatformDeviceSettingsSnapshot();
    Diagnostics = FGamePlatformDeviceSettingsDiagnostics();
    ScopeId.Invalidate();
    ++Generation;
    bPublishing = false;

    Super::Deinitialize();
}

FGamePlatformDeviceSettingsSnapshot
UGamePlatformDeviceSettingsSubsystem::GetSnapshot() const
{
    return Snapshot;
}

FGamePlatformDeviceSettingsDiagnostics
UGamePlatformDeviceSettingsSubsystem::GetDiagnostics() const
{
    FGamePlatformDeviceSettingsDiagnostics Result = Diagnostics;
    Result.SubscriptionCount = Subscriptions.Num();
    return Result;
}

bool UGamePlatformDeviceSettingsSubsystem::IsMutationEnvironmentAllowed() const
{
    if (IsRunningDedicatedServer() || IsRunningCommandlet() ||
        GetGameInstance() == nullptr)
    {
        return false;
    }

#if WITH_EDITOR
    // UGameUserSettings 是进程级对象；PIE 多实例不得互相覆盖显示／画质设置。
    if (GIsEditor && !FApp::IsGame())
    {
        return false;
    }
#endif

    return true;
}

bool UGamePlatformDeviceSettingsSubsystem::CanMutate(
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());

    if (!IsMutationEnvironmentAllowed())
    {
        ++Diagnostics.RejectedMutationCount;
        OutResult = FGamePlatformResult::Unsupported(
            TEXT("SettingsMutationEnvironmentUnsupported"),
            TEXT("当前环境不允许修改进程级客户端设备设置。"));
        return false;
    }

    if (bPublishing)
    {
        ++Diagnostics.RejectedMutationCount;
        OutResult = FGamePlatformResult::Failure(
            TEXT("SettingsCallbackReentrancy"),
            TEXT("设置状态回调期间禁止同步反入修改设置。"));
        return false;
    }

    OutResult = FGamePlatformResult::Success();
    return true;
}

bool UGamePlatformDeviceSettingsSubsystem::IsOwnerInScope(
    const TWeakObjectPtr<UObject>& Owner) const
{
    const UObject* Object = Owner.Get();
    const UGameInstance* Instance = GetGameInstance();
    if (!Object || !Instance ||
        Object->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
    {
        return false;
    }

    if (Object == Instance ||
        Object->GetTypedOuter<UGameInstance>() == Instance)
    {
        return true;
    }

    const UWorld* World = Object->GetWorld();
    return World && World->GetGameInstance() == Instance;
}

FGamePlatformResult
UGamePlatformDeviceSettingsSubsystem::ReadCurrentSettings(
    FGamePlatformDeviceSettings& OutSettings) const
{
    const UGameUserSettings* Native =
        UGameUserSettings::GetGameUserSettings();
    if (!Native)
    {
        return FGamePlatformResult::Failure(
            TEXT("GameUserSettingsUnavailable"),
            TEXT("UE用户设置对象不可用。"));
    }

    OutSettings = ReadNative(*Native);
    return FGamePlatformResult::Success();
}

FGamePlatformResult
UGamePlatformDeviceSettingsSubsystem::ApplyToEngine(
    const FGamePlatformDeviceSettings& Settings) const
{
    const FGamePlatformResult Validation =
        FGamePlatformDeviceSettingsPolicy::Validate(Settings);
    if (!Validation.IsSuccess())
    {
        return Validation;
    }

    UGameUserSettings* Native =
        UGameUserSettings::GetGameUserSettings();
    if (!Native)
    {
        return FGamePlatformResult::Failure(
            TEXT("GameUserSettingsUnavailable"),
            TEXT("UE用户设置对象不可用。"));
    }

    WriteNative(*Native, Settings);

    // 分辨率与非分辨率分别应用，避免 ApplySettings 自动保存导致预览无法回滚。
    Native->ApplyResolutionSettings(false);
    Native->ApplyNonResolutionSettings();
    return FGamePlatformResult::Success();
}

void UGamePlatformDeviceSettingsSubsystem::RefreshDerivedState()
{
    Snapshot.bHasStagedChanges =
        !FGamePlatformDeviceSettingsPolicy::AreEquivalent(
            Snapshot.Current,
            Snapshot.Staged);
}

void UGamePlatformDeviceSettingsSubsystem::PublishSnapshot()
{
    RefreshDerivedState();

    TArray<FGuid> SubscriptionIds;
    Subscriptions.GetKeys(SubscriptionIds);

    bPublishing = true;
    for (const FGuid& Id : SubscriptionIds)
    {
        FSubscriptionEntry* Entry = Subscriptions.Find(Id);
        if (!Entry)
        {
            continue;
        }

        if (!IsOwnerInScope(Entry->Owner))
        {
            Subscriptions.Remove(Id);
            continue;
        }

        // 复制回调后再调用，允许回调安全地延后撤销自身订阅。
        FGamePlatformDeviceSettingsChangedCallback Callback = Entry->Callback;
        if (Callback)
        {
            Callback(Snapshot);
            ++Diagnostics.SubscriberCallbackCount;
        }
    }
    bPublishing = false;
}

FGamePlatformResult
UGamePlatformDeviceSettingsSubsystem::ReloadFromDisk()
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (Snapshot.bPreviewActive)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsPreviewActive"),
            TEXT("显示设置预览期间不能从磁盘覆盖设置。"));
    }

    UGameUserSettings* Native =
        UGameUserSettings::GetGameUserSettings();
    if (!Native)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("GameUserSettingsUnavailable"),
            TEXT("UE用户设置对象不可用。"));
    }

    Native->LoadSettings(true);
    Native->ValidateSettings();
    Native->ApplyResolutionSettings(false);
    Native->ApplyNonResolutionSettings();
    Native->ConfirmVideoMode();

    Snapshot.Current = ReadNative(*Native);
    Snapshot.Staged = Snapshot.Current;
    ++Snapshot.Revision;
    ++Diagnostics.ApplyCount;
    PublishSnapshot();
    return FGamePlatformResult::Success();
}

FGamePlatformResult
UGamePlatformDeviceSettingsSubsystem::StageDeviceSettings(
    const FGamePlatformDeviceSettings& Settings)
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (Snapshot.bPreviewActive)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsPreviewActive"),
            TEXT("预览期间不能覆盖暂存设置，请先确认或回滚。"));
    }

    const FGamePlatformResult Validation =
        FGamePlatformDeviceSettingsPolicy::Validate(Settings);
    if (!Validation.IsSuccess())
    {
        ++Diagnostics.RejectedMutationCount;
        return Validation;
    }

    if (FGamePlatformDeviceSettingsPolicy::AreEquivalent(
        Snapshot.Staged,
        Settings))
    {
        return FGamePlatformResult::Success();
    }

    Snapshot.Staged = Settings;
    ++Snapshot.Revision;
    ++Diagnostics.StageCount;
    PublishSnapshot();
    return FGamePlatformResult::Success();
}

FGamePlatformResult
UGamePlatformDeviceSettingsSubsystem::ApplyStagedSettings(
    const EGamePlatformDeviceSettingsApplyMode ApplyMode)
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (Snapshot.bPreviewActive)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsPreviewActive"),
            TEXT("已有预览等待确认或回滚。"));
    }

    const FGamePlatformResult Validation =
        FGamePlatformDeviceSettingsPolicy::Validate(Snapshot.Staged);
    if (!Validation.IsSuccess())
    {
        ++Diagnostics.RejectedMutationCount;
        return Validation;
    }

    FGamePlatformDeviceSettings CurrentBeforeApply;
    const FGamePlatformResult ReadResult =
        ReadCurrentSettings(CurrentBeforeApply);
    if (!ReadResult.IsSuccess())
    {
        ++Diagnostics.RejectedMutationCount;
        return ReadResult;
    }

    if (ApplyMode == EGamePlatformDeviceSettingsApplyMode::Preview)
    {
        PreviewBaseline = CurrentBeforeApply;
    }

    const FGamePlatformResult ApplyResult =
        ApplyToEngine(Snapshot.Staged);
    if (!ApplyResult.IsSuccess())
    {
        ++Diagnostics.RejectedMutationCount;
        return ApplyResult;
    }

    UGameUserSettings* Native =
        UGameUserSettings::GetGameUserSettings();
    if (!Native)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("GameUserSettingsUnavailable"),
            TEXT("应用完成后UE用户设置对象意外不可用。"));
    }

    Snapshot.Current = ReadNative(*Native);
    Snapshot.Staged = Snapshot.Current;
    ++Diagnostics.ApplyCount;

    if (ApplyMode == EGamePlatformDeviceSettingsApplyMode::Preview)
    {
        Snapshot.bPreviewActive = true;
    }
    else
    {
        Native->ConfirmVideoMode();
        Native->SaveSettings();
        Snapshot.bPreviewActive = false;
        ++Diagnostics.SaveCount;
    }

    ++Snapshot.Revision;
    PublishSnapshot();
    return FGamePlatformResult::Success();
}

FGamePlatformResult
UGamePlatformDeviceSettingsSubsystem::ConfirmPreview()
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (!Snapshot.bPreviewActive)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsPreviewNotActive"),
            TEXT("当前没有等待确认的设置预览。"));
    }

    UGameUserSettings* Native =
        UGameUserSettings::GetGameUserSettings();
    if (!Native)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("GameUserSettingsUnavailable"),
            TEXT("UE用户设置对象不可用。"));
    }

    Native->ConfirmVideoMode();
    Native->SaveSettings();

    Snapshot.Current = ReadNative(*Native);
    Snapshot.Staged = Snapshot.Current;
    Snapshot.bPreviewActive = false;
    ++Snapshot.Revision;
    ++Diagnostics.SaveCount;
    PublishSnapshot();
    return FGamePlatformResult::Success();
}

FGamePlatformResult
UGamePlatformDeviceSettingsSubsystem::CancelPreview()
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (!Snapshot.bPreviewActive)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsPreviewNotActive"),
            TEXT("当前没有可回滚的设置预览。"));
    }

    const FGamePlatformResult RestoreResult =
        ApplyToEngine(PreviewBaseline);
    if (!RestoreResult.IsSuccess())
    {
        ++Diagnostics.RejectedMutationCount;
        return RestoreResult;
    }

    UGameUserSettings* Native =
        UGameUserSettings::GetGameUserSettings();
    if (!Native)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("GameUserSettingsUnavailable"),
            TEXT("回滚后UE用户设置对象意外不可用。"));
    }

    // 恢复预览前确认状态，但不写盘；磁盘仍保持预览前已保存值。
    Native->ConfirmVideoMode();
    Snapshot.Current = ReadNative(*Native);
    Snapshot.Staged = Snapshot.Current;
    Snapshot.bPreviewActive = false;
    ++Snapshot.Revision;
    ++Diagnostics.PreviewCancelCount;
    PublishSnapshot();
    return FGamePlatformResult::Success();
}

FGamePlatformResult
UGamePlatformDeviceSettingsSubsystem::DiscardStagedSettings()
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (Snapshot.bPreviewActive)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsPreviewActive"),
            TEXT("预览期间请使用CancelPreview回滚。"));
    }

    if (FGamePlatformDeviceSettingsPolicy::AreEquivalent(
        Snapshot.Current,
        Snapshot.Staged))
    {
        return FGamePlatformResult::Success();
    }

    Snapshot.Staged = Snapshot.Current;
    ++Snapshot.Revision;
    PublishSnapshot();
    return FGamePlatformResult::Success();
}

FGamePlatformDeviceSettingsSubscription
UGamePlatformDeviceSettingsSubsystem::Subscribe(
    TWeakObjectPtr<UObject> Owner,
    FGamePlatformDeviceSettingsChangedCallback Callback,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());

    if (!IsOwnerInScope(Owner) || !Callback)
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("SettingsSubscriptionInvalid"),
            TEXT("设置订阅需要当前GameInstance内的有效Owner和回调。"));
        return {};
    }

    if (Subscriptions.Num() >= MaxSettingsSubscriptions)
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("SettingsSubscriptionCapacity"),
            TEXT("设置订阅已达到实例安全上限。"));
        return {};
    }

    FGamePlatformDeviceSettingsSubscription Handle;
    Handle.ScopeId = ScopeId;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = Generation;

    FSubscriptionEntry Entry;
    Entry.Owner = Owner;
    Entry.Callback = MoveTemp(Callback);
    Subscriptions.Add(Handle.Id, MoveTemp(Entry));

    FGamePlatformDeviceSettingsChangedCallback InitialCallback =
        Subscriptions.FindChecked(Handle.Id).Callback;
    bPublishing = true;
    InitialCallback(Snapshot);
    ++Diagnostics.SubscriberCallbackCount;
    bPublishing = false;

    OutResult = FGamePlatformResult::Success();
    return Handle;
}

bool UGamePlatformDeviceSettingsSubsystem::Unsubscribe(
    const FGamePlatformDeviceSettingsSubscription& Subscription)
{
    check(IsInGameThread());

    if (!Subscription.IsValid() ||
        Subscription.ScopeId != ScopeId ||
        Subscription.Generation != Generation)
    {
        return false;
    }

    return Subscriptions.Remove(Subscription.Id) == 1;
}

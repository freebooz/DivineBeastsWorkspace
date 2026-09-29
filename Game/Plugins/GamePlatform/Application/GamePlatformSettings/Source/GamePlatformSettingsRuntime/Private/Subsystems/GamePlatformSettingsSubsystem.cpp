#include "Subsystems/GamePlatformSettingsSubsystem.h"

#include "Async/Async.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "GamePlatformSettingsLog.h"
#include "Interfaces/IGamePlatformSettingsProvider.h"
#include "Migration/GamePlatformSettingsMigrationRunner.h"
#include "Registry/GamePlatformSettingsRegistry.h"
#include "Resolution/GamePlatformSettingsResolver.h"
#include "Settings/GamePlatformSettingsProjectSettings.h"
#include "Validation/GamePlatformSettingsValidation.h"

UGamePlatformSettingsSubsystem::UGamePlatformSettingsSubsystem() = default;
UGamePlatformSettingsSubsystem::UGamePlatformSettingsSubsystem(
    FVTableHelper& Helper)
    : Super(Helper)
{
}
UGamePlatformSettingsSubsystem::~UGamePlatformSettingsSubsystem() = default;

bool UGamePlatformSettingsSubsystem::ShouldCreateSubsystem(
    UObject* Outer) const
{
    return !IsRunningCommandlet() &&
        Cast<UGameInstance>(Outer) != nullptr &&
        Super::ShouldCreateSubsystem(Outer);
}

void UGamePlatformSettingsSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    ScopeId = FGuid::NewGuid();
    Generation = 1;
    MutationGeneration = 1;
    Registry = MakeUnique<FGamePlatformSettingsRegistry>();

    IModularFeatures& Features = IModularFeatures::Get();
    FeatureRegisteredHandle =
        Features.OnModularFeatureRegistered().AddUObject(
            this,
            &UGamePlatformSettingsSubsystem::HandleModularFeatureRegistered);
    FeatureUnregisteredHandle =
        Features.OnModularFeatureUnregistered().AddUObject(
            this,
            &UGamePlatformSettingsSubsystem::HandleModularFeatureUnregistered);

    const FGamePlatformResult Result =
        ReloadInternal(EGamePlatformSettingsChangeReason::Reload);
    if (!Result.IsSuccess())
    {
        // 只记录错误码和脱敏说明；具体敏感设置值永不进入日志。
        UE_LOG(
            LogGamePlatformSettings,
            Error,
            TEXT("Settings initialization fallback: %s / %s"),
            *Result.Code.ToString(),
            *Result.Message);
    }
}

void UGamePlatformSettingsSubsystem::Deinitialize()
{
    bDeinitializing = true;
    ++Generation;

    IModularFeatures& Features = IModularFeatures::Get();
    if (FeatureRegisteredHandle.IsValid())
    {
        Features.OnModularFeatureRegistered().Remove(FeatureRegisteredHandle);
        FeatureRegisteredHandle.Reset();
    }
    if (FeatureUnregisteredHandle.IsValid())
    {
        Features.OnModularFeatureUnregistered().Remove(FeatureUnregisteredHandle);
        FeatureUnregisteredHandle.Reset();
    }

    // 异步保存不可强杀；Completion 只捕获弱子系统，销毁后不会回写 UObject。
    bSaveInFlight = false;
    bPendingTopologyReload = false;
    Subscriptions.Reset();
    Layers.Reset();
    Registry.Reset();
    Snapshot = FGamePlatformSettingsSnapshot();
    Diagnostics = FGamePlatformSettingsRuntimeDiagnostics();
    ScopeId.Invalidate();
    bPublishing = false;
    bUserDirty = false;
    bPendingResolve = false;

    Super::Deinitialize();
}

EGamePlatformSettingRuntimeScope
UGamePlatformSettingsSubsystem::GetCurrentRuntimeScope() const
{
    return IsRunningDedicatedServer()
        ? EGamePlatformSettingRuntimeScope::Server
        : EGamePlatformSettingRuntimeScope::Client;
}

bool UGamePlatformSettingsSubsystem::CanMutate(
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());

    if (bDeinitializing || !Registry)
    {
        ++Diagnostics.RejectedMutationCount;
        OutResult = FGamePlatformResult::Failure(
            TEXT("SettingsServiceUnavailable"),
            TEXT("设置服务尚未初始化或正在销毁。"));
        return false;
    }

    if (bPublishing)
    {
        ++Diagnostics.RejectedMutationCount;
        OutResult = FGamePlatformResult::Failure(
            TEXT("SettingsCallbackReentrancy"),
            TEXT("设置事件回调期间禁止同步反入修改设置。"));
        return false;
    }

    OutResult = FGamePlatformResult::Success();
    return true;
}

bool UGamePlatformSettingsSubsystem::IsOwnerInScope(
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

bool UGamePlatformSettingsSubsystem::IsPublicMutableLayer(
    const EGamePlatformSettingLayer Layer) const
{
    if (GetCurrentRuntimeScope() == EGamePlatformSettingRuntimeScope::Server)
    {
        return Layer == EGamePlatformSettingLayer::Session;
    }

    return Layer == EGamePlatformSettingLayer::User ||
        Layer == EGamePlatformSettingLayer::Session;
}

bool UGamePlatformSettingsSubsystem::GetValue(
    const FName SettingId,
    FGamePlatformSettingValue& OutValue) const
{
    const FGamePlatformResolvedSetting* Found =
        Snapshot.Values.Find(SettingId);
    if (!Found)
    {
        return false;
    }

    OutValue = Found->Value;
    return true;
}

bool UGamePlatformSettingsSubsystem::GetDescriptor(
    const FName SettingId,
    FGamePlatformSettingDescriptor& OutDescriptor) const
{
    if (!Registry)
    {
        return false;
    }

    const FGamePlatformSettingDescriptor* Found =
        Registry->GetDescriptors().Find(SettingId);
    if (!Found)
    {
        return false;
    }

    OutDescriptor = *Found;
    return true;
}

FGamePlatformResult UGamePlatformSettingsSubsystem::SetValue(
    const FName SettingId,
    const EGamePlatformSettingLayer Layer,
    const FGamePlatformSettingValue& Value)
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (!IsPublicMutableLayer(Layer))
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsLayerReadOnly"),
            TEXT("公共SetValue只能修改User/Session等允许的运行时覆盖层。"));
    }

    const FGamePlatformSettingDescriptor* Descriptor =
        Registry->GetDescriptors().Find(SettingId);
    if (!Descriptor)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsSettingIdUnknown"),
            TEXT("SettingId未注册。"));
    }

    if (!FGamePlatformSettingsValidation::IsLayerAllowed(
            *Descriptor, Layer, GetCurrentRuntimeScope()))
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsLayerNotAllowed"),
            TEXT("该设置不允许写入指定作用域/端侧配置层。"));
    }

    const FGamePlatformResult Validation =
        FGamePlatformSettingsValidation::ValidateValue(*Descriptor, Value);
    if (!Validation.IsSuccess())
    {
        ++Diagnostics.RejectedMutationCount;
        return Validation;
    }

    TMap<FName, FGamePlatformSettingValue>& LayerValues =
        Layers.FindOrAdd(Layer);
    if (const FGamePlatformSettingValue* Existing =
            LayerValues.Find(SettingId);
        Existing && Existing->Equals(Value))
    {
        return FGamePlatformResult::Success();
    }

    LayerValues.Add(SettingId, Value);
    ++MutationGeneration;
    bPendingResolve = true;
    if (Layer == EGamePlatformSettingLayer::User)
    {
        bUserDirty = true;
    }
    Snapshot.bDirty = bUserDirty || bPendingResolve;
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformSettingsSubsystem::ResetValue(
    const FName SettingId,
    const EGamePlatformSettingLayer Layer)
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (!IsPublicMutableLayer(Layer))
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsLayerReadOnly"),
            TEXT("公共ResetValue不能修改只读配置层。"));
    }

    TMap<FName, FGamePlatformSettingValue>* LayerValues =
        Layers.Find(Layer);
    if (!LayerValues || LayerValues->Remove(SettingId) == 0)
    {
        return FGamePlatformResult::Success();
    }

    ++MutationGeneration;
    bPendingResolve = true;
    if (Layer == EGamePlatformSettingLayer::User)
    {
        bUserDirty = true;
    }
    Snapshot.bDirty = bUserDirty || bPendingResolve;
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformSettingsSubsystem::ResetCategory(
    const FName Category,
    const EGamePlatformSettingLayer Layer)
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (Category.IsNone() || !IsPublicMutableLayer(Layer))
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsResetCategoryInvalid"),
            TEXT("ResetCategory需要有效Category和可变配置层。"));
    }

    TMap<FName, FGamePlatformSettingValue>* LayerValues =
        Layers.Find(Layer);
    if (!LayerValues)
    {
        return FGamePlatformResult::Success();
    }

    TArray<FName> ToRemove;
    for (const TPair<FName, FGamePlatformSettingValue>& Pair : *LayerValues)
    {
        const FGamePlatformSettingDescriptor* Descriptor =
            Registry->GetDescriptors().Find(Pair.Key);
        if (Descriptor && Descriptor->Category == Category)
        {
            ToRemove.Add(Pair.Key);
        }
    }

    if (ToRemove.IsEmpty())
    {
        return FGamePlatformResult::Success();
    }

    for (const FName SettingId : ToRemove)
    {
        LayerValues->Remove(SettingId);
    }

    ++MutationGeneration;
    bPendingResolve = true;
    if (Layer == EGamePlatformSettingLayer::User)
    {
        bUserDirty = true;
    }
    Snapshot.bDirty = bUserDirty || bPendingResolve;
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformSettingsSubsystem::Apply(
    const EGamePlatformSettingsChangeReason Reason)
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    return ResolveAndPublish(Reason, false);
}

void UGamePlatformSettingsSubsystem::BuildDefaultLayers(
    const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
    TMap<EGamePlatformSettingLayer,
        TMap<FName, FGamePlatformSettingValue>>& OutLayers) const
{
    OutLayers.Reset();
    for (const TPair<FName, FGamePlatformSettingDescriptor>& Pair : Descriptors)
    {
        OutLayers.FindOrAdd(Pair.Value.DefaultLayer)
            .Add(Pair.Key, Pair.Value.DefaultValue);
    }
}

FGamePlatformResult
UGamePlatformSettingsSubsystem::ResolvePersistenceProvider(
    IGamePlatformSettingsPersistenceProvider*& OutProvider) const
{
    OutProvider = nullptr;

    TArray<IGamePlatformSettingsPersistenceProvider*> Providers =
        IModularFeatures::Get()
            .GetModularFeatureImplementations<
                IGamePlatformSettingsPersistenceProvider>(
                    IGamePlatformSettingsPersistenceProvider::
                        GetModularFeatureName());

    for (IGamePlatformSettingsPersistenceProvider* Provider : Providers)
    {
        if (!Provider ||
            !Provider->SupportsRuntime(GetCurrentRuntimeScope()))
        {
            continue;
        }

        if (OutProvider)
        {
            OutProvider = nullptr;
            return FGamePlatformResult::Failure(
                TEXT("SettingsPersistenceAmbiguous"),
                TEXT("当前端侧存在多个持久化Provider，拒绝按加载顺序选择。"));
        }

        OutProvider = Provider;
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult
UGamePlatformSettingsSubsystem::SanitizePersistencePayload(
    const TMap<FName, FGamePlatformSettingDescriptor>& Descriptors,
    FGamePlatformSettingsPersistencePayload& InOutPayload) const
{
    const UGamePlatformSettingsProjectSettings* ProjectSettings =
        GetDefault<UGamePlatformSettingsProjectSettings>();
    const bool bStrict =
        !ProjectSettings || ProjectSettings->bStrictValidation;

    TMap<EGamePlatformSettingLayer,
        TMap<FName, FGamePlatformSettingValue>> Sanitized;

    for (const TPair<
        EGamePlatformSettingLayer,
        TMap<FName, FGamePlatformSettingValue>>& LayerPair : InOutPayload.Layers)
    {
        for (const TPair<FName, FGamePlatformSettingValue>& ValuePair :
            LayerPair.Value)
        {
            const FGamePlatformSettingDescriptor* Descriptor =
                Descriptors.Find(ValuePair.Key);
            const bool bLayerAllowed =
                Descriptor &&
                FGamePlatformSettingsValidation::IsLayerAllowed(
                    *Descriptor,
                    LayerPair.Key,
                    GetCurrentRuntimeScope());
            const FGamePlatformResult ValueValidation =
                Descriptor
                    ? FGamePlatformSettingsValidation::ValidateValue(
                        *Descriptor,
                        ValuePair.Value)
                    : FGamePlatformResult::Failure(
                        TEXT("SettingsSettingIdUnknown"),
                        TEXT("持久层包含未注册SettingId。"));

            if (!Descriptor || !bLayerAllowed ||
                !ValueValidation.IsSuccess())
            {
                if (bStrict)
                {
                    return FGamePlatformResult::Failure(
                        TEXT("SettingsPersistenceValueInvalid"),
                        FString::Printf(
                            TEXT("持久层设置 %s 未通过严格校验。"),
                            *ValuePair.Key.ToString()));
                }
                continue;
            }

            Sanitized.FindOrAdd(LayerPair.Key)
                .Add(ValuePair.Key, ValuePair.Value);
        }
    }

    InOutPayload.Layers = MoveTemp(Sanitized);
    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformSettingsSubsystem::ApplyMigrations(
    FGamePlatformSettingsPersistencePayload& InOutPayload,
    const int32 TargetVersion)
{
    TArray<IGamePlatformSettingsMigration*> Migrations =
        IModularFeatures::Get()
            .GetModularFeatureImplementations<
                IGamePlatformSettingsMigration>(
                    IGamePlatformSettingsMigration::GetModularFeatureName());

    TMap<FName, FGamePlatformSettingValue>& UserValues =
        InOutPayload.Layers.FindOrAdd(EGamePlatformSettingLayer::User);
    int32 AppliedMigrationCount = 0;

    const FGamePlatformResult Result =
        FGamePlatformSettingsMigrationRunner::Run(
            UserValues,
            InOutPayload.SchemaVersion,
            TargetVersion,
            Migrations,
            AppliedMigrationCount);
    if (Result.IsSuccess())
    {
        Diagnostics.MigrationCount += AppliedMigrationCount;
    }
    return Result;
}

FGamePlatformResult UGamePlatformSettingsSubsystem::Reload()
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (bSaveInFlight)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsSaveInFlight"),
            TEXT("异步保存进行中，暂不允许Reload覆盖内存层。"));
    }

    return ReloadInternal(EGamePlatformSettingsChangeReason::Reload);
}

FGamePlatformResult UGamePlatformSettingsSubsystem::ReloadInternal(
    const EGamePlatformSettingsChangeReason Reason)
{
    check(IsInGameThread());

    const UGamePlatformSettingsProjectSettings* ProjectSettings =
        GetDefault<UGamePlatformSettingsProjectSettings>();
    const int32 MaxProviders =
        ProjectSettings ? ProjectSettings->MaxProviders : 64;
    const int32 MaxDescriptors =
        ProjectSettings ? ProjectSettings->MaxDescriptors : 1024;
    const int32 TargetVersion =
        ProjectSettings ? ProjectSettings->SchemaVersion : 1;

    TUniquePtr<FGamePlatformSettingsRegistry> NewRegistry =
        MakeUnique<FGamePlatformSettingsRegistry>();
    const FGamePlatformResult RegistryResult =
        NewRegistry->Rebuild(
            MaxProviders,
            MaxDescriptors,
            GetCurrentRuntimeScope());
    if (!RegistryResult.IsSuccess())
    {
        Snapshot.LastResult = RegistryResult;
        return RegistryResult;
    }

    TMap<EGamePlatformSettingLayer,
        TMap<FName, FGamePlatformSettingValue>> NewLayers;
    BuildDefaultLayers(NewRegistry->GetDescriptors(), NewLayers);

    FGamePlatformResult PersistenceResult =
        FGamePlatformResult::Success();
    IGamePlatformSettingsPersistenceProvider* Persistence = nullptr;
    PersistenceResult = ResolvePersistenceProvider(Persistence);
    if (!PersistenceResult.IsSuccess())
    {
        Snapshot.LastResult = PersistenceResult;
        return PersistenceResult;
    }

    FGamePlatformSettingsPersistencePayload Payload;
    Payload.SchemaVersion = TargetVersion;

    if (Persistence)
    {
        PersistenceResult =
            Persistence->Load(NewRegistry->GetDescriptors(), Payload);
        if (PersistenceResult.IsSuccess())
        {
            const FGamePlatformResult SanitizeResult =
                SanitizePersistencePayload(
                    NewRegistry->GetDescriptors(),
                    Payload);
            if (!SanitizeResult.IsSuccess())
            {
                PersistenceResult = SanitizeResult;
            }
        }

        if (PersistenceResult.IsSuccess())
        {
            const FGamePlatformResult MigrationResult =
                ApplyMigrations(Payload, TargetVersion);
            if (!MigrationResult.IsSuccess())
            {
                PersistenceResult = MigrationResult;
            }
        }
    }

    if (PersistenceResult.IsSuccess())
    {
        for (TPair<
            EGamePlatformSettingLayer,
            TMap<FName, FGamePlatformSettingValue>>& Pair : Payload.Layers)
        {
            TMap<FName, FGamePlatformSettingValue>& Destination =
                NewLayers.FindOrAdd(Pair.Key);
            for (TPair<FName, FGamePlatformSettingValue>& ValuePair :
                Pair.Value)
            {
                Destination.Add(ValuePair.Key, MoveTemp(ValuePair.Value));
            }
        }
    }
    // 持久层损坏/迁移失败不删除原文件；继续使用默认值形成可运行Snapshot并返回失败。

    Registry = MoveTemp(NewRegistry);
    Layers = MoveTemp(NewLayers);
    bUserDirty = false;
    bPendingResolve = true;
    ++MutationGeneration;

    Diagnostics.ProviderCount = Registry->GetProviderCount();
    Diagnostics.DescriptorCount = Registry->GetDescriptors().Num();

    const FGamePlatformResult ResolveResult =
        ResolveAndPublish(Reason, true);
    if (!ResolveResult.IsSuccess())
    {
        return ResolveResult;
    }

    if (!PersistenceResult.IsSuccess())
    {
        Snapshot.LastResult = PersistenceResult;

        FGamePlatformSettingsChangeSet Empty;
        Empty.Reason = Reason;
        Empty.Revision = Snapshot.Revision;
        PublishChanges(Empty);
        return PersistenceResult;
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformSettingsSubsystem::ResolveAndPublish(
    const EGamePlatformSettingsChangeReason Reason,
    const bool bForceNotification)
{
    if (!Registry)
    {
        return FGamePlatformResult::Failure(
            TEXT("SettingsRegistryUnavailable"),
            TEXT("设置注册表不可用。"));
    }

    FGamePlatformSettingsSnapshot Candidate;
    FGamePlatformSettingsChangeSet Changes;
    const FGamePlatformResult Result =
        FGamePlatformSettingsResolver::Resolve(
            Registry->GetDescriptors(),
            Layers,
            GetCurrentRuntimeScope(),
            Snapshot,
            Reason,
            GetDefault<UGamePlatformSettingsProjectSettings>()
                ? GetDefault<UGamePlatformSettingsProjectSettings>()->SchemaVersion
                : 1,
            bUserDirty,
            Candidate,
            Changes);
    if (!Result.IsSuccess())
    {
        ++Diagnostics.RejectedMutationCount;
        Snapshot.LastResult = Result;
        return Result;
    }

    Candidate.bSaveInFlight = bSaveInFlight;
    Candidate.bDirty = bUserDirty;
    Candidate.LastResult = FGamePlatformResult::Success();
    Snapshot = MoveTemp(Candidate);
    bPendingResolve = false;

    ++Diagnostics.ResolveCount;
    if (Reason == EGamePlatformSettingsChangeReason::Apply ||
        Reason == EGamePlatformSettingsChangeReason::Reset)
    {
        ++Diagnostics.ApplyCount;
    }

    if (bForceNotification || !Changes.Changes.IsEmpty())
    {
        PublishChanges(Changes);
    }

    return FGamePlatformResult::Success();
}

FGamePlatformResult UGamePlatformSettingsSubsystem::Save()
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (bSaveInFlight)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsSaveInFlight"),
            TEXT("已有异步设置保存正在执行。"));
    }

    if (bPendingResolve)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsApplyRequiredBeforeSave"),
            TEXT("存在尚未Apply的设置修改，必须先生成最终Snapshot。"));
    }

    if (!bUserDirty)
    {
        return FGamePlatformResult::Success();
    }

    IGamePlatformSettingsPersistenceProvider* Persistence = nullptr;
    const FGamePlatformResult ProviderResult =
        ResolvePersistenceProvider(Persistence);
    if (!ProviderResult.IsSuccess())
    {
        return ProviderResult;
    }
    if (!Persistence)
    {
        return FGamePlatformResult::Unsupported(
            TEXT("SettingsPersistenceUnavailable"),
            TEXT("当前端侧没有设置持久化Provider。"));
    }

    const TMap<FName, FGamePlatformSettingValue>* UserLayer =
        Layers.Find(EGamePlatformSettingLayer::User);
    static const TMap<FName, FGamePlatformSettingValue> EmptyUserLayer;
    const TMap<FName, FGamePlatformSettingValue>& Values =
        UserLayer ? *UserLayer : EmptyUserLayer;

    const uint64 SavedGeneration = MutationGeneration;
    const TWeakObjectPtr<UGamePlatformSettingsSubsystem> WeakThis(this);
    const int32 Version =
        GetDefault<UGamePlatformSettingsProjectSettings>()
            ? GetDefault<UGamePlatformSettingsProjectSettings>()->SchemaVersion
            : 1;

    const FGamePlatformResult StartResult =
        Persistence->BeginSave(
            Values,
            Version,
            [WeakThis, SavedGeneration](const FGamePlatformResult& Result)
            {
                auto Complete = [WeakThis, SavedGeneration, Result]()
                {
                    if (UGamePlatformSettingsSubsystem* Self = WeakThis.Get())
                    {
                        Self->HandleSaveCompleted(SavedGeneration, Result);
                    }
                };

                if (IsInGameThread())
                {
                    Complete();
                }
                else
                {
                    AsyncTask(ENamedThreads::GameThread, MoveTemp(Complete));
                }
            });

    if (!StartResult.IsSuccess())
    {
        return StartResult;
    }

    bSaveInFlight = true;
    Snapshot.bSaveInFlight = true;
    ++Snapshot.Revision;
    ++Diagnostics.SaveRequestCount;

    FGamePlatformSettingsChangeSet Empty;
    Empty.Reason = EGamePlatformSettingsChangeReason::Persistence;
    Empty.Revision = Snapshot.Revision;
    PublishChanges(Empty);
    return FGamePlatformResult::Success();
}

void UGamePlatformSettingsSubsystem::HandleSaveCompleted(
    const uint64 SavedMutationGeneration,
    const FGamePlatformResult& Result)
{
    check(IsInGameThread());

    if (bDeinitializing)
    {
        return;
    }

    bSaveInFlight = false;
    Snapshot.bSaveInFlight = false;
    Snapshot.LastResult = Result;

    const bool bSavedCurrentGeneration =
        Result.IsSuccess() &&
        SavedMutationGeneration == MutationGeneration;
    if (bSavedCurrentGeneration)
    {
        bUserDirty = false;
    }

    Snapshot.bDirty = bUserDirty || bPendingResolve;
    ++Snapshot.Revision;

    FGamePlatformSettingsChangeSet Empty;
    Empty.Reason = EGamePlatformSettingsChangeReason::Persistence;
    Empty.Revision = Snapshot.Revision;
    PublishChanges(Empty);

    // Provider拓扑变化可能发生在异步保存期间。只有确认保存覆盖当前MutationGeneration时才安全重载；
    // 保存失败或保存期间又发生新修改时继续保留待重载标志，避免Reload丢失尚未持久化的用户值。
    if (bPendingTopologyReload && bSavedCurrentGeneration)
    {
        bPendingTopologyReload = false;
        const FGamePlatformResult ReloadResult =
            ReloadInternal(EGamePlatformSettingsChangeReason::ProviderChanged);
        if (!ReloadResult.IsSuccess())
        {
            UE_LOG(
                LogGamePlatformSettings,
                Error,
                TEXT("Deferred settings topology reload failed: %s / %s"),
                *ReloadResult.Code.ToString(),
                *ReloadResult.Message);
        }
    }
}

FGamePlatformResult UGamePlatformSettingsSubsystem::SwitchUserContext(
    const FString& UserContextKey)
{
    FGamePlatformResult Guard;
    if (!CanMutate(Guard))
    {
        return Guard;
    }

    if (GetCurrentRuntimeScope() != EGamePlatformSettingRuntimeScope::Client)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Unsupported(
            TEXT("SettingsUserContextServerUnsupported"),
            TEXT("User Profile上下文只适用于客户端。"));
    }

    if (UserContextKey.Len() > 128 ||
        UserContextKey.Contains(TEXT("\r")) ||
        UserContextKey.Contains(TEXT("\n")))
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsUserContextInvalid"),
            TEXT("用户上下文键必须是不超过128字符且不含换行的稳定不透明值。"));
    }

    if (bSaveInFlight || bUserDirty || bPendingResolve)
    {
        ++Diagnostics.RejectedMutationCount;
        return FGamePlatformResult::Failure(
            TEXT("SettingsUserContextSwitchBlocked"),
            TEXT("切换用户上下文前必须完成Apply与Save，并等待异步保存结束。"));
    }

    if (CurrentUserContextKey == UserContextKey)
    {
        return FGamePlatformResult::Success();
    }

    IGamePlatformSettingsPersistenceProvider* Persistence = nullptr;
    const FGamePlatformResult ProviderResult =
        ResolvePersistenceProvider(Persistence);
    if (!ProviderResult.IsSuccess())
    {
        return ProviderResult;
    }
    if (!Persistence)
    {
        return FGamePlatformResult::Unsupported(
            TEXT("SettingsPersistenceUnavailable"),
            TEXT("当前客户端没有用户设置持久化Provider。"));
    }

    const FGamePlatformResult ContextResult =
        Persistence->SetUserContext(UserContextKey);
    if (!ContextResult.IsSuccess())
    {
        ++Diagnostics.RejectedMutationCount;
        return ContextResult;
    }

    CurrentUserContextKey = UserContextKey;
    ++MutationGeneration;
    return ReloadInternal(EGamePlatformSettingsChangeReason::Reload);
}

FGamePlatformSettingsSnapshot
UGamePlatformSettingsSubsystem::GetSnapshot() const
{
    return Snapshot;
}

FGamePlatformSettingsRuntimeDiagnostics
UGamePlatformSettingsSubsystem::GetDiagnostics() const
{
    FGamePlatformSettingsRuntimeDiagnostics Result = Diagnostics;
    Result.SubscriptionCount = Subscriptions.Num();
    return Result;
}

FGamePlatformSettingsRuntimeSubscription
UGamePlatformSettingsSubsystem::Subscribe(
    TWeakObjectPtr<UObject> Owner,
    FGamePlatformSettingsRuntimeChangedCallback Callback,
    FGamePlatformResult& OutResult)
{
    check(IsInGameThread());

    const UGamePlatformSettingsProjectSettings* ProjectSettings =
        GetDefault<UGamePlatformSettingsProjectSettings>();
    const int32 MaxSubscriptions =
        ProjectSettings ? ProjectSettings->MaxSubscriptions : 128;

    if (!IsOwnerInScope(Owner) || !Callback)
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("SettingsSubscriptionInvalid"),
            TEXT("设置订阅需要当前GameInstance内有效Owner和回调。"));
        return {};
    }

    if (Subscriptions.Num() >= MaxSubscriptions)
    {
        OutResult = FGamePlatformResult::Failure(
            TEXT("SettingsSubscriptionCapacity"),
            TEXT("设置订阅数量已达到实例安全上限。"));
        return {};
    }

    FGamePlatformSettingsRuntimeSubscription Handle;
    Handle.ScopeId = ScopeId;
    Handle.Id = FGuid::NewGuid();
    Handle.Generation = Generation;

    FSettingsRuntimeSubscriptionEntry Entry;
    Entry.Owner = Owner;
    Entry.Callback = MoveTemp(Callback);
    Subscriptions.Add(Handle.Id, MoveTemp(Entry));

    FGamePlatformSettingsChangeSet Initial;
    Initial.Reason = EGamePlatformSettingsChangeReason::Reload;
    Initial.Revision = Snapshot.Revision;

    FGamePlatformSettingsRuntimeChangedCallback InitialCallback =
        Subscriptions.FindChecked(Handle.Id).Callback;
    bPublishing = true;
    InitialCallback(Initial, Snapshot);
    ++Diagnostics.SubscriberCallbackCount;
    bPublishing = false;

    OutResult = FGamePlatformResult::Success();
    return Handle;
}

bool UGamePlatformSettingsSubsystem::Unsubscribe(
    const FGamePlatformSettingsRuntimeSubscription& Subscription)
{
    check(IsInGameThread());

    if (bPublishing ||
        !Subscription.IsValid() ||
        Subscription.ScopeId != ScopeId ||
        Subscription.Generation != Generation)
    {
        return false;
    }

    return Subscriptions.Remove(Subscription.Id) == 1;
}

void UGamePlatformSettingsSubsystem::PublishChanges(
    const FGamePlatformSettingsChangeSet& ChangeSet)
{
    TArray<FGuid> SubscriptionIds;
    Subscriptions.GetKeys(SubscriptionIds);

    bPublishing = true;
    for (const FGuid Id : SubscriptionIds)
    {
        FSettingsRuntimeSubscriptionEntry* Entry = Subscriptions.Find(Id);
        if (!Entry)
        {
            continue;
        }

        if (!IsOwnerInScope(Entry->Owner))
        {
            Subscriptions.Remove(Id);
            continue;
        }

        FGamePlatformSettingsRuntimeChangedCallback Callback =
            Entry->Callback;
        if (Callback)
        {
            Callback(ChangeSet, Snapshot);
            ++Diagnostics.SubscriberCallbackCount;
        }
    }
    bPublishing = false;
}

void UGamePlatformSettingsSubsystem::HandleModularFeatureRegistered(
    const FName& Type,
    IModularFeature* Feature)
{
    HandleFeatureTopologyChanged(Type);
}

void UGamePlatformSettingsSubsystem::HandleModularFeatureUnregistered(
    const FName& Type,
    IModularFeature* Feature)
{
    HandleFeatureTopologyChanged(Type);
}

void UGamePlatformSettingsSubsystem::HandleFeatureTopologyChanged(
    const FName& Type)
{
    if (bDeinitializing ||
        (Type != IGamePlatformSettingsProvider::GetModularFeatureName() &&
         Type !=
             IGamePlatformSettingsPersistenceProvider::GetModularFeatureName() &&
         Type != IGamePlatformSettingsMigration::GetModularFeatureName()))
    {
        return;
    }

    if (bSaveInFlight)
    {
        // 保存使用当前User层快照；此时立即重建Registry/Layers会制造保存代次与内存代次竞态。
        bPendingTopologyReload = true;
        return;
    }

    const FGamePlatformResult Result =
        ReloadInternal(EGamePlatformSettingsChangeReason::ProviderChanged);
    if (!Result.IsSuccess())
    {
        UE_LOG(
            LogGamePlatformSettings,
            Error,
            TEXT("Settings provider topology reload failed: %s / %s"),
            *Result.Code.ToString(),
            *Result.Message);
    }
}

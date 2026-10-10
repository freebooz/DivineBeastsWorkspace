// 平台GI设置执行：拥有当前层/解析注册表/独立Client持久化克隆；游戏线程事件驱动，异步保存弱回调和代次复核，卸载释放克隆。
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
    bDeinitializing = false;
    ++Generation;
    ++MutationGeneration;
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
    ++LoadGeneration;
    ++SaveRequestGeneration;
    bLoadInFlight = false;
    PendingLoadRegistry.Reset();

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
    bTopologyWakeQueued = false;
    Subscriptions.Reset();
    Layers.Reset();
    ScopedPersistenceProvider.Reset();
    PersistenceFactory = nullptr;
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
    FGamePlatformResult& OutResult, bool bAllowLoadInFlight)
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

    if (bLoadInFlight && !bAllowLoadInFlight)
    {
        ++Diagnostics.RejectedMutationCount;
        OutResult = FGamePlatformResult::Failure(TEXT("SettingsLoadInProgress"), TEXT("设置档案读取尚未完成；查询继续使用已发布只读快照。"));
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
    const uint64 ExpectedInstanceGeneration = Generation;
    const uint64 ExpectedLoadGeneration = LoadGeneration;
    const auto IsScopeCurrent = [this, ExpectedInstanceGeneration, ExpectedLoadGeneration]()
    { return !bDeinitializing && Generation == ExpectedInstanceGeneration && LoadGeneration == ExpectedLoadGeneration; };
    const auto Closed = []() { return FGamePlatformResult::Failure(TEXT("SettingsPreparationInvalidated"), TEXT("持久化Provider解析期间作用域已失效。")); };

    OutProvider = nullptr;

    TArray<IGamePlatformSettingsPersistenceProvider*> Providers =
        IModularFeatures::Get()
            .GetModularFeatureImplementations<
                IGamePlatformSettingsPersistenceProvider>(
                    IGamePlatformSettingsPersistenceProvider::
                        GetModularFeatureName());

    for (IGamePlatformSettingsPersistenceProvider* Provider : Providers)
    {
        if (!Provider) { continue; }
        const bool bSupportsRuntime = Provider->SupportsRuntime(GetCurrentRuntimeScope());
        if (!IsScopeCurrent()) { OutProvider = nullptr; return Closed(); }
        if (!bSupportsRuntime) { continue; }

        if (OutProvider)
        {
            OutProvider = nullptr;
            return FGamePlatformResult::Failure(
                TEXT("SettingsPersistenceAmbiguous"),
                TEXT("当前端侧存在多个持久化Provider，拒绝按加载顺序选择。"));
        }

        OutProvider = Provider;
    }

    if (GetCurrentRuntimeScope() == EGamePlatformSettingRuntimeScope::Client)
    {
        // 查询只解析Provider所有权，不改变读取事务代次；Reload/拓扑/关闭入口统一使旧请求失效。
        // 无可选持久层时仍需完成本次默认层候选，不能在捕获代次后再次推进而丢弃自己的终态。
        if (!OutProvider) { ScopedPersistenceProvider.Reset(); PersistenceFactory = nullptr; }
        else
        {
            if (PersistenceFactory != OutProvider || !ScopedPersistenceProvider)
            {
                auto Scoped = OutProvider->CreateScopedProvider();
                if (!IsScopeCurrent()) { OutProvider = nullptr; return Closed(); }
                if (!Scoped)
                {
                    OutProvider = nullptr;
                    return FGamePlatformResult::Unsupported(TEXT("SettingsScopedPersistenceRequired"), TEXT("客户端Provider必须提供GI独占实例，拒绝共享可变用户上下文。"));
                }
                const auto ContextResult = Scoped->SetUserContext(CurrentUserContextKey);
                if (!IsScopeCurrent()) { OutProvider = nullptr; return Closed(); }
                if (!ContextResult.IsSuccess()) { OutProvider = nullptr; return ContextResult; }
                PersistenceFactory = OutProvider;
                ScopedPersistenceProvider = MakeShareable(Scoped.Release());
            }
            OutProvider = ScopedPersistenceProvider.Get();
        }
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
    if (bDeinitializing || !Registry) { return FGamePlatformResult::Failure(TEXT("SettingsServiceUnavailable"), TEXT("设置作用域已关闭。")); }
    // 拓扑预检可能失败；仍须先撤销旧候选，防止旧Completion发布已撤回描述。
    const uint64 ExpectedInstanceGeneration = Generation;
    const uint64 ExpectedLoadGeneration = ++LoadGeneration;
    const auto IsCurrentPreparation = [this, ExpectedInstanceGeneration, ExpectedLoadGeneration]()
    { return !bDeinitializing && Generation == ExpectedInstanceGeneration && LoadGeneration == ExpectedLoadGeneration && static_cast<bool>(Registry); };
    const auto Invalidated = []() { return FGamePlatformResult::Failure(TEXT("SettingsPreparationInvalidated"), TEXT("读取准备期间作用域或候选代次已失效。")); };
    // 所有准备入口共用一次失败发布，直接Reload/延后唤醒也能撤销订阅者的Loading投影。
    // 外部Provider可同步关闭或启动新候选；过期准备栈只返回，不覆盖新代次的终态。
    const auto PublishPreparationFailure = [this, Reason, &IsCurrentPreparation](const FGamePlatformResult& Failure)
    {
        if (IsCurrentPreparation())
        {
            Snapshot.LastResult = Failure;
            FGamePlatformSettingsChangeSet Empty; Empty.Reason = Reason; Empty.Revision = Snapshot.Revision;
            PublishChanges(Empty);
        }
        return Failure;
    };
    bLoadInFlight = false; Snapshot.bLoading = false; PendingLoadRegistry.Reset();
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
    if (!IsCurrentPreparation()) { return Invalidated(); }
    if (!RegistryResult.IsSuccess()) { return PublishPreparationFailure(RegistryResult); }

    FGamePlatformResult PersistenceResult =
        FGamePlatformResult::Success();
    IGamePlatformSettingsPersistenceProvider* Persistence = nullptr;
    PersistenceResult = ResolvePersistenceProvider(Persistence);
    if (!IsCurrentPreparation()) { return Invalidated(); }
    if (!PersistenceResult.IsSuccess()) { return PublishPreparationFailure(PersistenceResult); }

    // 新的候选注册表只属于本次读取；拓扑重载使旧回调失效，不允许半份持久层先发布。
    PendingLoadRegistry = MoveTemp(NewRegistry);
    PendingLoadTargetVersion = TargetVersion;
    PendingLoadReason = Reason;
    bLoadInFlight = true;
    Snapshot.bLoading = true;
    if (!Persistence)
    {
        FGamePlatformSettingsPersistencePayload Payload; Payload.SchemaVersion = TargetVersion;
        HandleLoadCompleted(ExpectedLoadGeneration, MoveTemp(Payload), FGamePlatformResult::Success());
        return Snapshot.LastResult;
    }
    TWeakObjectPtr<UGamePlatformSettingsSubsystem> WeakThis(this);
    const auto ProviderOwner = ScopedPersistenceProvider;
    const auto DescriptorCopy = PendingLoadRegistry->GetDescriptors();
    const auto Accepted = Persistence->BeginLoad(DescriptorCopy,
        [WeakThis, ExpectedLoadGeneration](FGamePlatformSettingsPersistencePayload Payload, const FGamePlatformResult& Result)
        {
            check(IsInGameThread());
            if (auto* Self = WeakThis.Get()) { Self->HandleLoadCompleted(ExpectedLoadGeneration, MoveTemp(Payload), Result); }
        });
    if (!Accepted.IsSuccess() && bLoadInFlight && ExpectedLoadGeneration == LoadGeneration)
    { HandleLoadCompleted(ExpectedLoadGeneration, {}, Accepted); }
    else if (bLoadInFlight && ExpectedLoadGeneration == LoadGeneration)
    {
        FGamePlatformSettingsChangeSet Empty; Empty.Reason = Reason; Empty.Revision = Snapshot.Revision;
        PublishChanges(Empty);
    }
    return Accepted;
}

void UGamePlatformSettingsSubsystem::HandleLoadCompleted(uint64 ExpectedLoadGeneration,
    FGamePlatformSettingsPersistencePayload Payload, const FGamePlatformResult& Result)
{
    check(IsInGameThread());
    if (bDeinitializing || !bLoadInFlight || ExpectedLoadGeneration != LoadGeneration || !PendingLoadRegistry) { return; }
    // 先消费终态资格，迁移/事件重入或错误Provider重复Completion不能再消费同一候选。
    bLoadInFlight = false;
    Snapshot.bLoading = false;
    const auto Reason = PendingLoadReason;
    const int32 TargetVersion = PendingLoadTargetVersion;
    auto CandidateRegistry = MoveTemp(PendingLoadRegistry);
    auto FinalResult = Result;
    if (FinalResult.IsSuccess()) { FinalResult = SanitizePersistencePayload(CandidateRegistry->GetDescriptors(), Payload); }
    if (FinalResult.IsSuccess()) { FinalResult = ApplyMigrations(Payload, TargetVersion); }
    // 外部迁移Provider虽只获纯值合同，也可能触发拓扑事件；旧候选不得覆盖重载/换绑后的代次。
    if (bDeinitializing || ExpectedLoadGeneration != LoadGeneration) { return; }
    if (!FinalResult.IsSuccess())
    {
        // Reload失败保留已发布层；SwitchUserContext已在启动新读取前撤销旧User层，避免账号泄漏。
        Snapshot.LastResult = FinalResult;
        FGamePlatformSettingsChangeSet Empty; Empty.Reason = Reason; Empty.Revision = Snapshot.Revision;
        PublishChanges(Empty); return;
    }
    TMap<EGamePlatformSettingLayer, TMap<FName, FGamePlatformSettingValue>> CandidateLayers;
    BuildDefaultLayers(CandidateRegistry->GetDescriptors(), CandidateLayers);
    for (auto& Pair : Payload.Layers)
    {
        auto& Destination = CandidateLayers.FindOrAdd(Pair.Key);
        for (auto& ValuePair : Pair.Value) { Destination.Add(ValuePair.Key, MoveTemp(ValuePair.Value)); }
    }
    FGamePlatformSettingsSnapshot CandidateSnapshot;
    FGamePlatformSettingsChangeSet Changes;
    FinalResult = FGamePlatformSettingsResolver::Resolve(CandidateRegistry->GetDescriptors(), CandidateLayers,
        GetCurrentRuntimeScope(), Snapshot, Reason, TargetVersion, false, CandidateSnapshot, Changes);
    if (bDeinitializing || ExpectedLoadGeneration != LoadGeneration) { return; }
    if (!FinalResult.IsSuccess())
    {
        Snapshot.LastResult = FinalResult;
        FGamePlatformSettingsChangeSet Empty; Empty.Reason = Reason; Empty.Revision = Snapshot.Revision;
        PublishChanges(Empty); return;
    }
    // 所有校验/迁移/解析成功后一次性替换注册表、层和快照；任何失败保留之前的可读状态。
    Registry = MoveTemp(CandidateRegistry); Layers = MoveTemp(CandidateLayers);
    CandidateSnapshot.bLoading = false; CandidateSnapshot.bSaveInFlight = bSaveInFlight;
    CandidateSnapshot.LastResult = FGamePlatformResult::Success(); Snapshot = MoveTemp(CandidateSnapshot);
    bUserDirty = false; bPendingResolve = false; ++MutationGeneration; ++Diagnostics.ResolveCount;
    Diagnostics.ProviderCount = Registry->GetProviderCount(); Diagnostics.DescriptorCount = Registry->GetDescriptors().Num();
    PublishChanges(Changes);
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

    Candidate.bLoading = bLoadInFlight;
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
    if (!CanMutate(Guard)) { return Guard; }
    if (bSaveInFlight) { return FGamePlatformResult::Failure(TEXT("SettingsSaveInFlight"), TEXT("已有保存正在执行。")); }
    if (bPendingResolve) { return FGamePlatformResult::Failure(TEXT("SettingsApplyRequiredBeforeSave"), TEXT("保存前必须先Apply生成快照。")); }
    if (!bUserDirty) { return FGamePlatformResult::Success(); }
    IGamePlatformSettingsPersistenceProvider* Persistence = nullptr;
    const auto ProviderResult = ResolvePersistenceProvider(Persistence);
    if (!ProviderResult.IsSuccess()) { return ProviderResult; }
    if (!Persistence) { return FGamePlatformResult::Unsupported(TEXT("SettingsPersistenceUnavailable"), TEXT("当前端侧没有持久化Provider。")); }
    // Provider可同步完成并触发退出；持有克隆和纯值副本，先登记终态身份再进入外部代码。
    const auto ProviderOwner = ScopedPersistenceProvider;
    const auto* UserLayer = Layers.Find(EGamePlatformSettingLayer::User);
    const TMap<FName, FGamePlatformSettingValue> Values = UserLayer ? *UserLayer : TMap<FName, FGamePlatformSettingValue>();
    const uint64 ExpectedInstanceGeneration = Generation;
    const uint64 ExpectedSaveRequestGeneration = ++SaveRequestGeneration;
    const uint64 SavedGeneration = MutationGeneration;
    bSaveInFlight = true; Snapshot.bSaveInFlight = true;
    ++Snapshot.Revision; ++Diagnostics.SaveRequestCount;
    FGamePlatformSettingsChangeSet Empty; Empty.Reason = EGamePlatformSettingsChangeReason::Persistence; Empty.Revision = Snapshot.Revision;
    PublishChanges(Empty);
    if (bDeinitializing || Generation != ExpectedInstanceGeneration || !bSaveInFlight || SaveRequestGeneration != ExpectedSaveRequestGeneration)
    { return FGamePlatformResult::Failure(TEXT("SettingsServiceUnavailable"), TEXT("保存受理前作用域已失效。")); }
    const TWeakObjectPtr<UGamePlatformSettingsSubsystem> WeakThis(this);
    const auto* ProjectSettings = GetDefault<UGamePlatformSettingsProjectSettings>();
    const auto StartResult = Persistence->BeginSave(Values, ProjectSettings ? ProjectSettings->SchemaVersion : 1,
        [WeakThis, ExpectedInstanceGeneration, ExpectedSaveRequestGeneration, SavedGeneration](const FGamePlatformResult& Result)
        {
            auto Complete = [WeakThis, ExpectedInstanceGeneration, ExpectedSaveRequestGeneration, SavedGeneration, Result]()
            { if (auto* Self = WeakThis.Get()) { Self->HandleSaveCompleted(ExpectedInstanceGeneration, ExpectedSaveRequestGeneration, SavedGeneration, Result); } };
            if (IsInGameThread()) { Complete(); } else { AsyncTask(ENamedThreads::GameThread, MoveTemp(Complete)); }
        });
    if (!StartResult.IsSuccess()) { HandleSaveCompleted(ExpectedInstanceGeneration, ExpectedSaveRequestGeneration, SavedGeneration, StartResult); }
    return StartResult;
}


void UGamePlatformSettingsSubsystem::HandleSaveCompleted(
    const uint64 ExpectedInstanceGeneration,
    const uint64 ExpectedSaveRequestGeneration,
    const uint64 SavedMutationGeneration,
    const FGamePlatformResult& Result)
{
    check(IsInGameThread());

    if (bDeinitializing || Generation != ExpectedInstanceGeneration || SaveRequestGeneration != ExpectedSaveRequestGeneration || !bSaveInFlight)
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
    if (bDeinitializing || Generation != ExpectedInstanceGeneration || SaveRequestGeneration != ExpectedSaveRequestGeneration) { return; }

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
    if (!CanMutate(Guard, true))
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

    const auto ProviderOwner = ScopedPersistenceProvider;
    const uint64 ExpectedGeneration = Generation;
    const FGamePlatformResult ContextResult =
        Persistence->SetUserContext(UserContextKey);
    if (bDeinitializing || Generation != ExpectedGeneration) { return FGamePlatformResult::Failure(TEXT("SettingsServiceUnavailable"), TEXT("用户换绑期间作用域已关闭。")); }
    if (!ContextResult.IsSuccess())
    {
        ++Diagnostics.RejectedMutationCount;
        return ContextResult;
    }

    // 校验/Provider上下文切换成功才取消旧读取消费者；非法/相同键请求不会中断合法在飞读取。
    ++LoadGeneration; bLoadInFlight = false; Snapshot.bLoading = false; PendingLoadRegistry.Reset();
    CurrentUserContextKey = UserContextKey;
    // 账号切换不能在读取新档案时继续展示上个账号User层；先事件驱动发布默认/Session投影。
    Layers.Remove(EGamePlatformSettingLayer::User);
    bPendingResolve = true;
    ResolveAndPublish(EGamePlatformSettingsChangeReason::Reload, true);
    if (bDeinitializing || Generation != ExpectedGeneration) { return FGamePlatformResult::Failure(TEXT("SettingsServiceUnavailable"), TEXT("用户投影通知期间作用域已关闭。")); }
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

    const auto PublishedSnapshot = Snapshot;
    const uint64 ExpectedGeneration = Generation;
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
            Callback(ChangeSet, PublishedSnapshot);
            if (bDeinitializing || ExpectedGeneration != Generation) { return; }
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
    // 先释放克隆，避免卸载/重新注册同地址工厂时错误复用旧用户状态。
    if (Type == IGamePlatformSettingsPersistenceProvider::GetModularFeatureName() && Feature == PersistenceFactory)
    { ++LoadGeneration; bLoadInFlight = false; Snapshot.bLoading = false; PendingLoadRegistry.Reset(); ScopedPersistenceProvider.Reset(); PersistenceFactory = nullptr; }
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

    // 拓扑通知到达即失效旧候选；即使广播中仅延后重建，旧Completion也不能在唤醒前发布。
    ++LoadGeneration; bLoadInFlight = false; Snapshot.bLoading = false; PendingLoadRegistry.Reset();
    if (bPublishing)
    {
        bPendingTopologyReload = true;
        if (!bTopologyWakeQueued)
        {
            bTopologyWakeQueued = true;
            TWeakObjectPtr<UGamePlatformSettingsSubsystem> WeakThis(this);
            const uint64 ExpectedGeneration = Generation;
            AsyncTask(ENamedThreads::GameThread, [WeakThis, ExpectedGeneration]()
            {
                auto* Self = WeakThis.Get();
                if (!Self || Self->bDeinitializing || Self->Generation != ExpectedGeneration) { return; }
                Self->bTopologyWakeQueued = false;
                if (Self->bPendingTopologyReload && !Self->bPublishing && !Self->bSaveInFlight && !Self->bUserDirty && !Self->bPendingResolve)
                { Self->bPendingTopologyReload = false; Self->ReloadInternal(EGamePlatformSettingsChangeReason::ProviderChanged); }
            });
        }
        return;
    }

    if (bSaveInFlight || bUserDirty || bPendingResolve)
    {
        // 保存使用当前User层快照；此时立即重建Registry/Layers会制造保存代次与内存代次竞态。
        bPendingTopologyReload = true;
        return;
    }

    const FGamePlatformResult Result =
        ReloadInternal(EGamePlatformSettingsChangeReason::ProviderChanged);
    if (!Result.IsSuccess())
    {
        // ReloadInternal统一拥有准备失败的终态事件，本调用方仅记录诊断，避免重复通知。
        UE_LOG(
            LogGamePlatformSettings,
            Error,
            TEXT("Settings provider topology reload failed: %s / %s"),
            *Result.Code.ToString(),
            *Result.Message);
    }
}

#include "Components/GamePlatformExperienceComponent.h"
#include "Definitions/GamePlatformExperienceDefinition.h"
#include "Definitions/GamePlatformPawnDefinition.h"
#include "Framework/GamePlatformGameModeBase.h"
#include "Framework/GamePlatformPlayerStateBase.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Settings/GamePlatformGameplaySettings.h"
#include "GamePlatformGameplay.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Net/UnrealNetwork.h"

namespace
{
FGamePlatformResult Error(FName Code)
{ return FGamePlatformResult::Failure(Code, TEXT("体验请求未满足当前生命周期或资源条件，请检查脱敏错误码。")); }
IGamePlatformDataService* DataFor(const UActorComponent& Component)
{
    UWorld* World = Component.GetWorld();
    UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
    return GI ? IGamePlatformDataService::Get(*GI) : nullptr;
}
struct FFactory
{
    FGamePlatformGameplayRegistration Registration;
    TWeakObjectPtr<UObject> Owner;
    FGamePlatformAssemblyFactory Create;
};
struct FAssembly
{
    FGamePlatformExperienceAssemblyEntry Spec;
    TUniquePtr<IGamePlatformExperienceAssembly> Object;
    bool bStarted = false;
    bool bActivated = false;
    bool bSkipped = false;
};
struct FObserver
{
    FGamePlatformGameplayRegistration Registration;
    TWeakObjectPtr<UObject> Owner;
    TFunction<void(const FGamePlatformExperienceSnapshot&)> Callback;
    int64 LastRevision = -1;
};
}

struct FGamePlatformExperienceRuntime
{
    FGuid Scope = FGuid::NewGuid();
    FGamePlatformDataLease Root;
    FGamePlatformDataLease Pawn;
    TArray<FGamePlatformDataLease> Extra;
    TMap<FName, FFactory> Factories;
    TArray<TUniquePtr<FAssembly>> Assemblies;
    TArray<FObserver> Observers;
    FGamePlatformClientExperienceSnapshot Local;
    bool bDependenciesRequested = false;
    bool bAllowDevelopment = false;
    bool bClosing = false;
    bool bExternalCall = false;
    double DeadlineSeconds = 0;
};

UGamePlatformExperienceComponent::UGamePlatformExperienceComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickInterval = 0.05f;
    Runtime = MakeUnique<FGamePlatformExperienceRuntime>();
}
UGamePlatformExperienceComponent::UGamePlatformExperienceComponent(FVTableHelper& Helper) : Super(Helper) {}
UGamePlatformExperienceComponent::~UGamePlatformExperienceComponent() = default;
void UGamePlatformExperienceComponent::BeginPlay() { Super::BeginPlay(); }
void UGamePlatformExperienceComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{ Super::GetLifetimeReplicatedProps(Out); DOREPLIFETIME(UGamePlatformExperienceComponent, Snapshot); }

FGamePlatformResult UGamePlatformExperienceComponent::BeginExperience(const FPrimaryAssetId& Id, bool bAllowDevelopment)
{
    check(IsInGameThread());
    if (!GetOwner() || !GetOwner()->HasAuthority() || !GetWorld() || Runtime->bClosing || Runtime->bExternalCall)
        return Error(TEXT("ExperienceAuthorityOrScopeInvalid"));
    if (Snapshot.Stage != EGamePlatformExperienceStage::Unassigned)
        return FGamePlatformResult::Unsupported(TEXT("ExperienceHotSwitchUnsupported"), TEXT("本版每个世界只启动一次体验，另一体验须正常退出后在新世界启动。"));
    FGamePlatformId Parsed;
    if (Id.PrimaryAssetType != UGamePlatformPrimaryDataAsset::DefinitionAssetType()
        || !FGamePlatformId::TryParse(Id.PrimaryAssetName.ToString(), Parsed)) return Error(TEXT("InvalidExperienceId"));
    auto* WorldService = IGamePlatformWorldService::Get(*GetWorld());
    if (!WorldService || !DataFor(*this)) return Error(TEXT("GameplayPrerequisiteMissing"));
    const auto World = WorldService->GetReadiness();
    if (!World.Context.ContextGeneration.IsValid()) return Error(TEXT("WorldIdentityUnavailable"));
    Snapshot.WorldContextGeneration = World.Context.ContextGeneration;
    Snapshot.ExperienceId = Parsed;
    Snapshot.ExperienceEpoch = 1;
    Runtime->bAllowDevelopment = bAllowDevelopment && !UE_BUILD_SHIPPING && !IsRunningCommandlet()
        && GetWorld()->GetNetMode() == NM_Standalone
        && FParse::Param(FCommandLine::Get(), TEXT("FoundationGameplayOffline"));
    SetStage(EGamePlatformExperienceStage::Preparing);
    StartLocalLoad(Id);
    return Snapshot.Stage == EGamePlatformExperienceStage::Failed ? Error(Snapshot.FailureCode) : FGamePlatformResult::Success();
}

void UGamePlatformExperienceComponent::StartLocalLoad(const FPrimaryAssetId& Id)
{
    auto* Data = DataFor(*this);
    if (!Data) { Fail(TEXT("DataUnavailable")); return; }
    Runtime->Local.ExperienceEpoch = Snapshot.ExperienceEpoch;
    Runtime->Local.Stage = EGamePlatformClientExperienceStage::Preparing;
    Runtime->Local.FailureCode = NAME_None;
    Runtime->DeadlineSeconds = FPlatformTime::Seconds() + GetDefault<UGamePlatformGameplaySettings>()->MaximumPhaseTimeoutSeconds;
    FGamePlatformResult Accepted;
    // 不让异步闭包持有组件或改变新代次；统一在组件采样点读取本次仍持有的精确租约。
    Runtime->Root = Data->AcquireDefinition(Id, UGamePlatformExperienceDefinition::StaticClass(), {TEXT("GameplayShared")},
        EGamePlatformDataLifetime::World, this, [](const auto&, const auto&) {}, Accepted);
    if (!Accepted.IsSuccess()) Fail(Accepted.Code);
}

const UGamePlatformExperienceDefinition* UGamePlatformExperienceComponent::GetLoadedExperience() const
{
    auto* Data = DataFor(*this);
    return Data ? Cast<UGamePlatformExperienceDefinition>(Data->GetLoadedDefinition(Runtime->Root)) : nullptr;
}
const UGamePlatformPawnDefinition* UGamePlatformExperienceComponent::GetLoadedDefaultPawn() const
{
    auto* Data = DataFor(*this);
    return Data ? Cast<UGamePlatformPawnDefinition>(Data->GetLoadedDefinition(Runtime->Pawn)) : nullptr;
}

void UGamePlatformExperienceComponent::AdvancePreparation()
{
    if (Runtime->bClosing || Runtime->Local.Stage != EGamePlatformClientExperienceStage::Preparing) return;
    if (FPlatformTime::Seconds() >= Runtime->DeadlineSeconds) { Fail(TEXT("ExperiencePreparationTimedOut")); return; }
    auto* Data = DataFor(*this);
    if (!Data) { Fail(TEXT("DataUnavailable")); return; }
    auto CheckLease = [&](const FGamePlatformDataLease& Lease) -> int32
    {
        const auto State = Data->GetLeaseState(Lease);
        if (State == EGamePlatformDataRequestState::Loading) return 0;
        if (State != EGamePlatformDataRequestState::Succeeded || !Data->GetLoadedDefinition(Lease)) return -1;
        return 1;
    };
    const int32 RootState = CheckLease(Runtime->Root);
    if (RootState < 0) { Fail(TEXT("ExperienceLeaseFailed")); return; }
    if (RootState == 0) return;
    const auto* Definition = GetLoadedExperience();
    if (!Definition || Definition->LogicalId != Snapshot.ExperienceId) { Fail(TEXT("ExperienceTypeOrIdentityMismatch")); return; }
    const auto Validation = Definition->ValidateDefinition();
    if (!Validation.IsSuccess()) { Fail(Validation.Code); return; }
    const bool bServer = GetOwner()->HasAuthority();
    if (bServer)
    {
        if (Definition->bDevelopmentOnly && !Runtime->bAllowDevelopment) { Fail(TEXT("DevelopmentExperienceNotAuthorized")); return; }
        auto* WorldService = IGamePlatformWorldService::Get(*GetWorld());
        if (!WorldService) { Fail(TEXT("WorldUnavailable")); return; }
        const auto World = WorldService->GetReadiness();
        if (World.Context.ContextGeneration != Snapshot.WorldContextGeneration || !World.bWorldNotTearingDown)
        { Fail(TEXT("WorldGenerationExpired")); return; }
        // 只读基础输入，故意不等待World的最终贡献者聚合，避免Gameplay->Loading->World互等。
        if (!World.bWorldObjectValid || !World.bDefinitionLoaded || !World.bMapIdentityMatched
            || !World.bSessionContextMatched || !World.bRequiredRegionsRegistered || !World.bRequiredStreamingReady) return;
        if (!Definition->SupportedWorldIds.Contains(World.Context.WorldId)) { Fail(TEXT("ExperienceWorldMismatch")); return; }
        if (Snapshot.ContentRevision == 0)
        { Snapshot.ContentRevision = Definition->DataVersion.ContentRevision; ++Snapshot.StateRevision; GetOwner()->ForceNetUpdate(); }
    }
    else if (Snapshot.ContentRevision == 0) return;
    else if (Snapshot.ContentRevision != Definition->DataVersion.ContentRevision)
    { Fail(TEXT("ExperienceContentRevisionMismatch")); return; }

    if (!Runtime->bDependenciesRequested)
    {
        Runtime->bDependenciesRequested = true;
        FGamePlatformResult Accepted;
        Runtime->Pawn = Data->AcquireDefinition(Definition->DefaultPawnDefinitionId, UGamePlatformPawnDefinition::StaticClass(),
            {TEXT("GameplayShared")}, EGamePlatformDataLifetime::World, this, [](const auto&, const auto&) {}, Accepted);
        if (!Accepted.IsSuccess()) { Fail(Accepted.Code); return; }
        if (bServer)
        {
            for (const auto& Id : Definition->ServerDefinitions)
            {
                auto Lease = Data->AcquireDefinition(Id, UGamePlatformDefinitionBase::StaticClass(), {TEXT("GameplayServer")},
                    EGamePlatformDataLifetime::World, this, [](const auto&, const auto&) {}, Accepted);
                Runtime->Extra.Add(Lease);
                if (!Accepted.IsSuccess()) { Fail(Accepted.Code); return; }
            }
            // 定义已通过无环验证；每轮选择依赖已入序的项，保留配置中的稳定顺序。
            TSet<FName> Ordered;
            while (Ordered.Num() < Definition->AssemblyEntries.Num())
                for (const auto& Entry : Definition->AssemblyEntries)
                    if (!Ordered.Contains(Entry.AssemblyId) && !Entry.Dependencies.ContainsByPredicate([&](FName Id) { return !Ordered.Contains(Id); }))
                    { auto Item = MakeUnique<FAssembly>(); Item->Spec = Entry; Runtime->Assemblies.Add(MoveTemp(Item)); Ordered.Add(Entry.AssemblyId); }
        }
        return;
    }
    bool bPending = false;
    for (const auto& Lease : Runtime->Extra)
    { const int32 State = CheckLease(Lease); if (State < 0) { Fail(TEXT("ServerDependencyFailed")); return; } bPending |= State == 0; }
    const int32 PawnState = CheckLease(Runtime->Pawn);
    if (PawnState < 0) { Fail(TEXT("PawnDefinitionLeaseFailed")); return; }
    if (bPending || PawnState == 0) return;
    const auto* PawnDefinition = GetLoadedDefaultPawn();
    UClass* PawnClass = PawnDefinition ? PawnDefinition->PawnClass.Get() : nullptr;
    if (!PawnDefinition || !PawnClass || !PawnClass->IsChildOf(APawn::StaticClass()) || PawnClass->HasAnyClassFlags(CLASS_Abstract))
    { Fail(TEXT("PawnClassNotPreloadedOrInvalid")); return; }
    if (bServer)
    {
        for (const auto& Item : Runtime->Assemblies)
        {
            if (Item->bActivated || Item->bSkipped) continue;
            auto* Factory = Runtime->Factories.Find(Item->Spec.FactoryId);
            FGamePlatformResult Result;
            if (!Factory || !Factory->Owner.IsValid()) Result = Error(TEXT("AssemblyFactoryMissing"));
            else
            {
                bool bFailedDependency = false;
                for (const auto& Previous : Runtime->Assemblies)
                    if (Item->Spec.Dependencies.Contains(Previous->Spec.AssemblyId) && Previous->bSkipped) bFailedDependency = true;
                if (bFailedDependency) Result = Error(TEXT("AssemblyDependencySkipped"));
                else
                {
                    TGuardValue<bool> Guard(Runtime->bExternalCall, true);
                    if (!Item->bStarted)
                    {
                        Item->bStarted = true;
                        Item->Object = Factory->Create();
                        Result = Item->Object ? Item->Object->BeginPrepare(*GetWorld(), Snapshot) : Error(TEXT("AssemblyFactoryReturnedNull"));
                    }
                    else Result = Item->Object ? Item->Object->PollPreparation() : Error(TEXT("AssemblyObjectMissing"));
                    if (Result.IsSuccess()) Result = Item->Object->Activate();
                }
            }
            if (Result.Status == EGamePlatformResultStatus::NotExecuted) return;
            if (!Result.IsSuccess())
            {
                if (Item->Spec.bRequired) { Fail(Result.Code.IsNone() ? FName(TEXT("AssemblyFailed")) : Result.Code); return; }
                if (Item->Object) { TGuardValue<bool> Guard(Runtime->bExternalCall, true); Item->Object->StopAccepting(); Item->Object->Release(); Item->Object.Reset(); }
                Item->bSkipped = true;
                UE_LOG(LogGamePlatformGameplay, Warning, TEXT("可选装配跳过：%s，原因：%s"), *Item->Spec.AssemblyId.ToString(), *Result.Code.ToString());
            }
            else Item->bActivated = true;
        }
    }
    Runtime->Local.Stage = EGamePlatformClientExperienceStage::Prepared;
    if (bServer) SetStage(EGamePlatformExperienceStage::Active);
}

void UGamePlatformExperienceComponent::SetStage(EGamePlatformExperienceStage Stage, FName ErrorCode)
{
    Snapshot.Stage = Stage; Snapshot.FailureCode = ErrorCode; ++Snapshot.StateRevision;
    if (GetOwner()) GetOwner()->ForceNetUpdate();
    UE_LOG(LogGamePlatformGameplay, Log, TEXT("Experience=%s Epoch=%lld Revision=%lld Stage=%d Error=%s"),
        *Snapshot.ExperienceId.ToString(), Snapshot.ExperienceEpoch, Snapshot.StateRevision, static_cast<int32>(Stage), *ErrorCode.ToString());
}
void UGamePlatformExperienceComponent::Fail(FName Code)
{
    if (Runtime->bClosing || Runtime->Local.Stage == EGamePlatformClientExperienceStage::Failed) return;
    // 先改变终态，之后任何同步释放回调都不能再推进当前运行。
    Runtime->Local.Stage = EGamePlatformClientExperienceStage::Failed;
    Runtime->Local.FailureCode = Code;
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        SetStage(EGamePlatformExperienceStage::Failed, Code);
        if (auto* Mode = GetWorld()->GetAuthGameMode<AGamePlatformGameModeBase>()) Mode->DrainPlayers(Code);
    }
    ReleaseResources();
}
void UGamePlatformExperienceComponent::ReleaseResources()
{
    for (int32 Index = Runtime->Assemblies.Num() - 1; Index >= 0; --Index)
        if (Runtime->Assemblies[Index]->Object)
        { TGuardValue<bool> Guard(Runtime->bExternalCall, true); Runtime->Assemblies[Index]->Object->StopAccepting(); Runtime->Assemblies[Index]->Object->Release(); }
    Runtime->Assemblies.Reset();
    if (auto* Data = DataFor(*this))
    {
        for (int32 Index = Runtime->Extra.Num() - 1; Index >= 0; --Index) if (Runtime->Extra[Index].IsValid()) Data->ReleaseDefinition(Runtime->Extra[Index]);
        if (Runtime->Pawn.IsValid()) Data->ReleaseDefinition(Runtime->Pawn);
        if (Runtime->Root.IsValid()) Data->ReleaseDefinition(Runtime->Root);
    }
    Runtime->Extra.Reset(); Runtime->Pawn = {}; Runtime->Root = {}; Runtime->bDependenciesRequested = false;
}
FGamePlatformResult UGamePlatformExperienceComponent::BeginExperienceDrain(FName Reason)
{
    check(IsInGameThread());
    if (!GetOwner() || !GetOwner()->HasAuthority() || Runtime->bExternalCall) return Error(TEXT("DrainAuthorityOrReentry"));
    if (Snapshot.Stage == EGamePlatformExperienceStage::Released) return FGamePlatformResult::Success();
    SetStage(EGamePlatformExperienceStage::Draining, Reason);
    if (auto* Mode = GetWorld()->GetAuthGameMode<AGamePlatformGameModeBase>()) Mode->DrainPlayers(Reason);
    Runtime->Local.Stage = EGamePlatformClientExperienceStage::Released;
    ReleaseResources();
    SetStage(EGamePlatformExperienceStage::Released, Reason);
    return FGamePlatformResult::Success();
}
void UGamePlatformExperienceComponent::OnRep_Snapshot()
{
    if (Runtime->bClosing || !GetWorld()) return;
    if (Snapshot.Stage == EGamePlatformExperienceStage::Draining || Snapshot.Stage == EGamePlatformExperienceStage::Released
        || Snapshot.Stage == EGamePlatformExperienceStage::Failed)
    {
        Runtime->Local.Stage = EGamePlatformClientExperienceStage::Released;
        ReleaseResources(); return;
    }
    if (Snapshot.IsAssigned() && Runtime->Local.ExperienceEpoch != Snapshot.ExperienceEpoch)
    {
        ReleaseResources();
        StartLocalLoad(FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), FName(*Snapshot.ExperienceId.ToString())));
    }
}
void UGamePlatformExperienceComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* Function)
{
    Super::TickComponent(DeltaTime, TickType, Function);
    if (Runtime->bClosing) return;
    if (!GetOwner()->HasAuthority()) OnRep_Snapshot();
    if (GetOwner()->HasAuthority() && Snapshot.IsServerActive())
        for (const auto& Item : Runtime->Assemblies)
            if (Item->bActivated)
            {
                const auto* Factory = Runtime->Factories.Find(Item->Spec.FactoryId);
                if (!Factory || !Factory->Owner.IsValid()) { Fail(TEXT("AssemblyOwnerExpired")); break; }
            }
    AdvancePreparation(); DispatchSnapshot();
}
void UGamePlatformExperienceComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetOwner() && GetOwner()->HasAuthority()) BeginExperienceDrain(TEXT("WorldEnded"));
    Runtime->bClosing = true; Runtime->Observers.Reset();
    ReleaseResources(); Runtime->Factories.Reset();
    Super::EndPlay(Reason);
}

FGamePlatformGameplayRegistration UGamePlatformExperienceComponent::RegisterAssemblyFactory(FName Id, TWeakObjectPtr<UObject> Owner,
    FGamePlatformAssemblyFactory Factory, FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (!GetOwner() || !GetOwner()->HasAuthority() || Runtime->bClosing || Runtime->bExternalCall || Id.IsNone()
        || Snapshot.Stage != EGamePlatformExperienceStage::Unassigned || !Owner.IsValid() || Owner->GetWorld() != GetWorld()
        || !Factory || Runtime->Factories.Contains(Id) || Runtime->Factories.Num() >= 64)
    { OutResult = Error(TEXT("AssemblyRegistrationRejected")); return {}; }
    FGamePlatformGameplayRegistration Handle{Runtime->Scope, FGuid::NewGuid()};
    Runtime->Factories.Add(Id, FFactory{Handle, Owner, MoveTemp(Factory)});
    OutResult = FGamePlatformResult::Success(); return Handle;
}
bool UGamePlatformExperienceComponent::UnregisterAssemblyFactory(const FGamePlatformGameplayRegistration& Handle)
{
    check(IsInGameThread());
    if (Runtime->bExternalCall || Handle.ScopeId != Runtime->Scope) return false;
    for (auto It = Runtime->Factories.CreateIterator(); It; ++It)
        if (It.Value().Registration.RegistrationId == Handle.RegistrationId)
        {
            It.RemoveCurrent();
            if (Snapshot.Stage == EGamePlatformExperienceStage::Preparing || Snapshot.IsServerActive()) Fail(TEXT("AssemblyFactoryRevoked"));
            return true;
        }
    return false;
}
FGamePlatformClientExperienceSnapshot UGamePlatformExperienceComponent::GetClientExperienceSnapshot() const { return Runtime->Local; }
FGamePlatformPlayerLifecycleSnapshot UGamePlatformExperienceComponent::GetPlayerLifecycleSnapshot(const APlayerState& Player) const
{
    const auto* State = Cast<AGamePlatformPlayerStateBase>(&Player);
    return State && State->GetWorld() == GetWorld() ? State->GetLifecycleSnapshot() : FGamePlatformPlayerLifecycleSnapshot{};
}
FGamePlatformGameplayRegistration UGamePlatformExperienceComponent::SubscribeExperienceChanged(TWeakObjectPtr<UObject> Owner,
    TFunction<void(const FGamePlatformExperienceSnapshot&)> Callback, FGamePlatformResult& OutResult)
{
    check(IsInGameThread());
    if (Runtime->bClosing || Runtime->bExternalCall || !Owner.IsValid() || Owner->GetWorld() != GetWorld()
        || !Callback || Runtime->Observers.Num() >= 128)
    { OutResult = Error(TEXT("GameplaySubscriptionRejected")); return {}; }
    FGamePlatformGameplayRegistration Handle{Runtime->Scope, FGuid::NewGuid()};
    Runtime->Observers.Add(FObserver{Handle, Owner, MoveTemp(Callback), -1});
    OutResult = FGamePlatformResult::Success(); return Handle;
}
bool UGamePlatformExperienceComponent::Unsubscribe(const FGamePlatformGameplayRegistration& Handle)
{
    check(IsInGameThread());
    if (Runtime->bExternalCall || Handle.ScopeId != Runtime->Scope) return false;
    return Runtime->Observers.RemoveAll([&](const auto& Entry) { return Entry.Registration.RegistrationId == Handle.RegistrationId; }) > 0;
}
void UGamePlatformExperienceComponent::DispatchSnapshot()
{
    Runtime->Observers.RemoveAll([](const auto& Entry) { return !Entry.Owner.IsValid(); });
    TGuardValue<bool> Guard(Runtime->bExternalCall, true);
    for (auto& Entry : Runtime->Observers)
        if (Entry.LastRevision != Snapshot.StateRevision)
        { Entry.LastRevision = Snapshot.StateRevision; Entry.Callback(Snapshot); }
}
FGamePlatformGameplayDiagnostics UGamePlatformExperienceComponent::GetDiagnostics() const
{
    FGamePlatformGameplayDiagnostics Result;
    Result.Experience = Snapshot; Result.LocalResources = Runtime->Local;
    Result.HeldLeases = static_cast<int32>(Runtime->Root.IsValid()) + static_cast<int32>(Runtime->Pawn.IsValid()) + Runtime->Extra.Num();
    Result.AssemblyCount = Runtime->Assemblies.Num(); Result.ObserverCount = Runtime->Observers.Num();
    return Result;
}

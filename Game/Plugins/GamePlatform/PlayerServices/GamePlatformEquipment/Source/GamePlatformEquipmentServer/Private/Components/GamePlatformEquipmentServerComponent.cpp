// 平台服务器装备作用域实现；终态和GAS所有权说明见同名公开组件。
#include "Components/GamePlatformEquipmentServerComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/GamePlatformEquipmentComponent.h"
#include "Definitions/GamePlatformEquipmentDefinition.h"
#include "Interfaces/GamePlatformEquipmentPersistencePort.h"
#include "GameFramework/Actor.h"
#include "UObject/StrongObjectPtr.h"

UGamePlatformEquipmentServerComponent::
UGamePlatformEquipmentServerComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UGamePlatformEquipmentServerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ShutdownEquipmentRuntime();
    Super::EndPlay(EndPlayReason);
}

void UGamePlatformEquipmentServerComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
    ShutdownEquipmentRuntime();
    Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UGamePlatformEquipmentServerComponent::ShutdownEquipmentRuntime()
{
    check(IsInGameThread());
    if (bRuntimeClosed) { return; }
    bRuntimeClosed = true;
    ++LifetimeGeneration;
    bReady = false;
    bPersistenceInFlight = false;
    RevokeAllRuntimeGrants();
    GrantPort.Reset();
    Persistence.Reset();
    AbilitySystem.Reset();
    StateComponent.Reset();
    Definitions.Reset();
    Snapshot = {};
    PlayerId.Reset();
    CharacterId.Reset();
}

bool UGamePlatformEquipmentServerComponent::InitializeEquipmentRuntime(
    const FString& InPlayerId,
    const FString& InCharacterId,
    UGamePlatformEquipmentComponent* InStateComponent,
    TSharedPtr<IGamePlatformEquipmentPersistencePort, ESPMode::ThreadSafe>
        InPersistence,
    TSharedPtr<FGamePlatformEquipmentGASGrantPort, ESPMode::ThreadSafe>
        InGrantPort)
{
    AActor* Owner = GetOwner();
    if (bRuntimeClosed || bApplyingRuntimeSnapshot || !Owner ||
        !Owner->HasAuthority() ||
        InPlayerId.IsEmpty() ||
        InCharacterId.IsEmpty() ||
        !IsValid(InStateComponent) ||
        !InPersistence.IsValid() ||
        !InGrantPort.IsValid() ||
        bPersistenceInFlight)
    {
        return false;
    }

    // 再初始化不会遗留上一用户/Avatar的能力；已在飞操作须完成后才能进入新代次。
    const uint64 BeforeRevoke = LifetimeGeneration;
    RevokeAllRuntimeGrants();
    if (bRuntimeClosed || BeforeRevoke != LifetimeGeneration) { return false; }
    ++LifetimeGeneration;
    Snapshot = {};
    PlayerId = InPlayerId;
    CharacterId = InCharacterId;
    StateComponent = InStateComponent;
    Persistence = MoveTemp(InPersistence);
    GrantPort = MoveTemp(InGrantPort);

    bReady = false;
    bRuntimeError = false;
    bPersistenceInFlight = true;
    LastError = EGamePlatformEquipmentError::None;

    const uint64 ExpectedLifetimeGeneration = LifetimeGeneration;
    const uint64 ExpectedRequestGeneration = ++PersistenceRequestGeneration;
    TWeakObjectPtr<UGamePlatformEquipmentServerComponent> WeakThis(this);

    const auto RequestPersistence = Persistence;
    const FString RequestPlayerId = PlayerId;
    const FString RequestCharacterId = CharacterId;
    const bool bStarted =
        RequestPersistence->BeginLoadEquipment(
            RequestPlayerId,
            RequestCharacterId,
            [WeakThis, ExpectedLifetimeGeneration, ExpectedRequestGeneration](
                FGamePlatformEquipmentSnapshot Loaded,
                EGamePlatformEquipmentError Error)
            {
                if (UGamePlatformEquipmentServerComponent* Self =
                    WeakThis.Get())
                {
                    if (Self->bRuntimeClosed || Self->LifetimeGeneration != ExpectedLifetimeGeneration || Self->PersistenceRequestGeneration != ExpectedRequestGeneration || !Self->bPersistenceInFlight) { return; }
                    Self->HandleLoadCompleted(
                        MoveTemp(Loaded),
                        Error);
                }
            });

    if (!bStarted && ExpectedRequestGeneration == PersistenceRequestGeneration && bPersistenceInFlight)
    {
        bPersistenceInFlight = false;
        return false;
    }

    return true;
}

void UGamePlatformEquipmentServerComponent::RegisterDefinition(
    UGamePlatformEquipmentDefinition* Definition)
{
    if (bRuntimeClosed || bApplyingRuntimeSnapshot) { return; }
    if (!IsValid(Definition) ||
        !Definition->IsStructurallyValid())
    {
        return;
    }

    Definitions.Add(
        Definition->EquipmentDefinitionId,
        Definition);
}

bool UGamePlatformEquipmentServerComponent::BindAvatar(
    UAbilitySystemComponent* InAbilitySystem,
    int32 InAvatarGeneration)
{
    if (bRuntimeClosed || !GetOwner() ||
        !GetOwner()->HasAuthority() ||
        !IsValid(InAbilitySystem))
    {
        return false;
    }

    const uint64 ExpectedLifetime = LifetimeGeneration;
    if (bApplyingRuntimeSnapshot) { return false; }
    if (AbilitySystem.Get() != InAbilitySystem ||
        AvatarGeneration != InAvatarGeneration)
    {
        RevokeAllRuntimeGrants();
    }

    if (bRuntimeClosed || ExpectedLifetime != LifetimeGeneration) { return false; }
    AbilitySystem = InAbilitySystem;
    AvatarGeneration = FMath::Max(0, InAvatarGeneration);

    if (Snapshot.EquipmentRevision <= 0)
    {
        return true;
    }

    const bool bApplied = ApplyRuntimeSnapshot(Snapshot);
    if (bRuntimeClosed || ExpectedLifetime != LifetimeGeneration) { return false; }
    if (!bApplied)
    {
        bRuntimeError = true;
        bReady = false;
        LastError = EGamePlatformEquipmentError::RuntimeApplyFailed;
        return false;
    }

    bRuntimeError = false;
    bReady = true;
    LastError = EGamePlatformEquipmentError::None;
    PublishSnapshot();
    return true;
}

EGamePlatformEquipmentError
UGamePlatformEquipmentServerComponent::RequestEquip(
    const FGamePlatformEquipRequest& Request)
{
    if (!bReady || bRuntimeError || !Persistence.IsValid())
    {
        return EGamePlatformEquipmentError::EquipmentNotLoaded;
    }

    if (bPersistenceInFlight)
    {
        return EGamePlatformEquipmentError::OperationInProgress;
    }

    if (!Request.OperationId.IsValid() ||
        Request.CharacterId != CharacterId ||
        Request.SlotId.IsNone() ||
        Request.ItemInstanceId.IsEmpty())
    {
        return EGamePlatformEquipmentError::Unauthorized;
    }

    if (Request.ExpectedEquipmentRevision !=
        Snapshot.EquipmentRevision)
    {
        return EGamePlatformEquipmentError::EquipmentRevisionConflict;
    }

    bool bKnownSlot = false;
    for (const TPair<
             FName,
             TObjectPtr<UGamePlatformEquipmentDefinition>>& Pair :
         Definitions)
    {
        if (IsValid(Pair.Value) &&
            Pair.Value->SupportsSlot(Request.SlotId))
        {
            bKnownSlot = true;
            break;
        }
    }

    if (!bKnownSlot)
    {
        return EGamePlatformEquipmentError::InvalidSlot;
    }

    bPersistenceInFlight = true;
    const uint64 ExpectedLifetimeGeneration = LifetimeGeneration;
    const uint64 ExpectedRequestGeneration = ++PersistenceRequestGeneration;
    TWeakObjectPtr<UGamePlatformEquipmentServerComponent> WeakThis(this);

    const auto RequestPersistence = Persistence;
    const FString RequestPlayerId = PlayerId;
    const FString RequestCharacterId = CharacterId;
    const auto RequestCopy = Request;
    const bool bStarted =
        RequestPersistence->BeginEquip(
            RequestPlayerId,
            RequestCopy,
            [WeakThis, ExpectedLifetimeGeneration, ExpectedRequestGeneration, OperationId = Request.OperationId](
                FGamePlatformEquipmentSnapshot Persisted,
                EGamePlatformEquipmentError Error)
            {
                if (UGamePlatformEquipmentServerComponent* Self =
                    WeakThis.Get())
                {
                    if (Self->bRuntimeClosed || Self->bApplyingRuntimeSnapshot || Self->LifetimeGeneration != ExpectedLifetimeGeneration || Self->PersistenceRequestGeneration != ExpectedRequestGeneration || !Self->bPersistenceInFlight) { return; }
                    Self->HandleMutationCompleted(
                        OperationId,
                        MoveTemp(Persisted),
                        Error);
                }
            });

    if (!bStarted && ExpectedRequestGeneration == PersistenceRequestGeneration && bPersistenceInFlight)
    {
        bPersistenceInFlight = false;
        return EGamePlatformEquipmentError::PersistenceUnavailable;
    }

    return EGamePlatformEquipmentError::None;
}

EGamePlatformEquipmentError
UGamePlatformEquipmentServerComponent::RequestUnequip(
    const FGamePlatformUnequipRequest& Request)
{
    if (!bReady || bRuntimeError || !Persistence.IsValid())
    {
        return EGamePlatformEquipmentError::EquipmentNotLoaded;
    }

    if (bPersistenceInFlight)
    {
        return EGamePlatformEquipmentError::OperationInProgress;
    }

    if (!Request.OperationId.IsValid() ||
        Request.CharacterId != CharacterId ||
        Request.SlotId.IsNone())
    {
        return EGamePlatformEquipmentError::Unauthorized;
    }

    if (Request.ExpectedEquipmentRevision !=
        Snapshot.EquipmentRevision)
    {
        return EGamePlatformEquipmentError::EquipmentRevisionConflict;
    }

    bPersistenceInFlight = true;
    const uint64 ExpectedLifetimeGeneration = LifetimeGeneration;
    const uint64 ExpectedRequestGeneration = ++PersistenceRequestGeneration;
    TWeakObjectPtr<UGamePlatformEquipmentServerComponent> WeakThis(this);

    const auto RequestPersistence = Persistence;
    const FString RequestPlayerId = PlayerId;
    const FString RequestCharacterId = CharacterId;
    const auto RequestCopy = Request;
    const bool bStarted =
        RequestPersistence->BeginUnequip(
            RequestPlayerId,
            RequestCopy,
            [WeakThis, ExpectedLifetimeGeneration, ExpectedRequestGeneration, OperationId = Request.OperationId](
                FGamePlatformEquipmentSnapshot Persisted,
                EGamePlatformEquipmentError Error)
            {
                if (UGamePlatformEquipmentServerComponent* Self =
                    WeakThis.Get())
                {
                    if (Self->bRuntimeClosed || Self->bApplyingRuntimeSnapshot || Self->LifetimeGeneration != ExpectedLifetimeGeneration || Self->PersistenceRequestGeneration != ExpectedRequestGeneration || !Self->bPersistenceInFlight) { return; }
                    Self->HandleMutationCompleted(
                        OperationId,
                        MoveTemp(Persisted),
                        Error);
                }
            });

    if (!bStarted && ExpectedRequestGeneration == PersistenceRequestGeneration && bPersistenceInFlight)
    {
        bPersistenceInFlight = false;
        return EGamePlatformEquipmentError::PersistenceUnavailable;
    }

    return EGamePlatformEquipmentError::None;
}

bool UGamePlatformEquipmentServerComponent::Reconcile()
{
    if (bRuntimeClosed || bApplyingRuntimeSnapshot || !Persistence.IsValid() ||
        PlayerId.IsEmpty() ||
        CharacterId.IsEmpty() ||
        bPersistenceInFlight)
    {
        return false;
    }

    bPersistenceInFlight = true;
    const uint64 ExpectedLifetimeGeneration = LifetimeGeneration;
    const uint64 ExpectedRequestGeneration = ++PersistenceRequestGeneration;
    TWeakObjectPtr<UGamePlatformEquipmentServerComponent> WeakThis(this);

    const auto RequestPersistence = Persistence;
    const FString RequestPlayerId = PlayerId;
    const FString RequestCharacterId = CharacterId;
    const bool bStarted =
        RequestPersistence->BeginLoadEquipment(
            RequestPlayerId,
            RequestCharacterId,
            [WeakThis, ExpectedLifetimeGeneration, ExpectedRequestGeneration](
                FGamePlatformEquipmentSnapshot Loaded,
                EGamePlatformEquipmentError Error)
            {
                if (UGamePlatformEquipmentServerComponent* Self =
                    WeakThis.Get())
                {
                    if (Self->bRuntimeClosed || Self->LifetimeGeneration != ExpectedLifetimeGeneration || Self->PersistenceRequestGeneration != ExpectedRequestGeneration || !Self->bPersistenceInFlight) { return; }
                    Self->HandleLoadCompleted(
                        MoveTemp(Loaded),
                        Error);
                }
            });

    if (!bStarted && ExpectedRequestGeneration == PersistenceRequestGeneration && bPersistenceInFlight)
    {
        bPersistenceInFlight = false;
    }

    return bStarted;
}

void UGamePlatformEquipmentServerComponent::HandleLoadCompleted(
    FGamePlatformEquipmentSnapshot Loaded,
    EGamePlatformEquipmentError Error)
{
    if (bRuntimeClosed) { return; }
    const uint64 ExpectedLifetime = LifetimeGeneration;
    bPersistenceInFlight = false;

    FString ValidationReason;
    if (Error != EGamePlatformEquipmentError::None ||
        !ValidateGamePlatformEquipmentSnapshot(Loaded, ValidationReason) ||
        Loaded.CharacterId != CharacterId ||
        Loaded.EquipmentRevision <= 0)
    {
        LastError = Error != EGamePlatformEquipmentError::None ? Error : EGamePlatformEquipmentError::InvalidResponse;
        bReady = false;
        bRuntimeError = true;
        return;
    }

    Snapshot = MoveTemp(Loaded);

    if (!IsValid(AbilitySystem.Get()))
    {
        bReady = false;
        bRuntimeError = false;
        LastError = EGamePlatformEquipmentError::AbilitySystemUnavailable;
        return;
    }

    const bool bApplied = ApplyRuntimeSnapshot(Snapshot);
    if (bRuntimeClosed || ExpectedLifetime != LifetimeGeneration) { return; }
    if (!bApplied)
    {
        bReady = false;
        bRuntimeError = true;
        LastError = EGamePlatformEquipmentError::RuntimeApplyFailed;
        return;
    }

    bReady = true;
    bRuntimeError = false;
    LastError = EGamePlatformEquipmentError::None;
    PublishSnapshot();
}


void UGamePlatformEquipmentServerComponent::HandleMutationCompleted(
    FGuid,
    FGamePlatformEquipmentSnapshot Persisted,
    EGamePlatformEquipmentError Error)
{
    if (bRuntimeClosed) { return; }
    const uint64 ExpectedLifetime = LifetimeGeneration;
    bPersistenceInFlight = false;

    if (Error != EGamePlatformEquipmentError::None)
    {
        LastError = Error;
        if (Error == EGamePlatformEquipmentError::EquipmentRevisionConflict ||
            Error == EGamePlatformEquipmentError::PersistenceOutcomeUnknown)
        {
            Reconcile();
        }
        return;
    }

    FString ValidationReason;
    if (!ValidateGamePlatformEquipmentSnapshot(Persisted, ValidationReason) ||
        Persisted.CharacterId != CharacterId ||
        Persisted.EquipmentRevision <= Snapshot.EquipmentRevision)
    {
        bRuntimeError = true;
        bReady = false;
        LastError = EGamePlatformEquipmentError::InvalidResponse;
        Reconcile();
        return;
    }

    Snapshot = Persisted;

    const bool bApplied = ApplyRuntimeSnapshot(Persisted);
    if (bRuntimeClosed || ExpectedLifetime != LifetimeGeneration) { return; }
    if (!bApplied)
    {
        bRuntimeError = true;
        bReady = false;
        LastError = EGamePlatformEquipmentError::RuntimeApplyFailed;
        return;
    }

    bRuntimeError = false;
    bReady = true;
    LastError = EGamePlatformEquipmentError::None;
    PublishSnapshot();
}


bool UGamePlatformEquipmentServerComponent::ApplyRuntimeSnapshot(const FGamePlatformEquipmentSnapshot& Persisted)
{
    check(IsInGameThread());
    if (bRuntimeClosed || bApplyingRuntimeSnapshot || !AbilitySystem.IsValid() || !GrantPort.IsValid()) { return false; }
    TGuardValue<bool> ApplyingGuard(bApplyingRuntimeSnapshot, true);
    // 候选复制和Definition保活防止Shutdown清空成员；Port独立保活直到局部授权全部撤销。
    TStrongObjectPtr<UGamePlatformEquipmentServerComponent> KeepSelf(this);
    const auto Candidate = Persisted;
    const auto Port = GrantPort;
    const auto WeakASC = AbilitySystem;
    const uint64 ExpectedLifetime = LifetimeGeneration;
    const int32 ExpectedAvatar = AvatarGeneration;
    auto ScopeCurrent = [this, WeakASC, ExpectedLifetime, ExpectedAvatar]()
    { return !bRuntimeClosed && LifetimeGeneration == ExpectedLifetime && AvatarGeneration == ExpectedAvatar && WeakASC.IsValid() && AbilitySystem == WeakASC; };
    FString Reason;
    if (!ValidateGamePlatformEquipmentSnapshot(Candidate, Reason)) { return false; }
    TArray<TStrongObjectPtr<UGamePlatformEquipmentDefinition>> CandidateDefinitions;
    for (const auto& Slot : Candidate.Slots)
    {
        UGamePlatformEquipmentDefinition* Definition = Definitions.FindRef(Slot.EquipmentDefinitionId).Get();
        if (!IsValid(Definition) || !Definition->IsStructurallyValid() || !Definition->SupportsSlot(Slot.SlotId) || Definition->CompatibleItemDefinitionId != Slot.ItemDefinitionId || Definition->VisualDefinitionId != Slot.VisualDefinitionId) { return false; }
        CandidateDefinitions.Emplace(Definition);
    }
    RevokeAllRuntimeGrants();
    if (!ScopeCurrent()) { return false; }
    TMap<FName, FGamePlatformEquipmentGameplayGrantHandle> NewHandles;
    auto Cleanup = [&]() { for (auto& Pair : NewHandles) { Port->Revoke(WeakASC.Get(), Pair.Value); } NewHandles.Reset(); };
    for (int32 Index = 0; Index < Candidate.Slots.Num(); ++Index)
    {
        FGamePlatformEquipmentGameplayGrantHandle Handle;
        const bool bGranted = Port->Grant(WeakASC.Get(), *CandidateDefinitions[Index], Handle, ScopeCurrent);
        if (!bGranted || !ScopeCurrent())
        { Port->Revoke(WeakASC.Get(), Handle); Cleanup(); return false; }
        NewHandles.Add(Candidate.Slots[Index].SlotId, MoveTemp(Handle));
    }
    if (!ScopeCurrent()) { Cleanup(); return false; }
    SlotGrantHandles = MoveTemp(NewHandles); ++EquipmentRuntimeGeneration;
    return true;
}


void UGamePlatformEquipmentServerComponent::RevokeAllRuntimeGrants()
{
    auto Handles = MoveTemp(SlotGrantHandles); SlotGrantHandles.Reset();
    const auto Port = GrantPort; const auto WeakASC = AbilitySystem;
    if (Port.IsValid()) { for (auto& Pair : Handles) { Port->Revoke(WeakASC.Get(), Pair.Value); } }
}


void UGamePlatformEquipmentServerComponent::PublishSnapshot()
{
    if (bRuntimeClosed) { return; }
    if (UGamePlatformEquipmentComponent* State =
        StateComponent.Get())
    {
        State->SetServerSnapshots(
            Snapshot,
            EquipmentRuntimeGeneration);
    }
}

// 平台服务器装备作用域实现；终态和GAS所有权说明见同名公开组件。
#include "Components/GamePlatformEquipmentServerComponent.h"

#include "AbilitySystemComponent.h"
#include "Components/GamePlatformEquipmentComponent.h"
#include "Definitions/GamePlatformEquipmentDefinition.h"
#include "Interfaces/GamePlatformEquipmentPersistencePort.h"

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
    if (bRuntimeClosed || !Owner ||
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
    RevokeAllRuntimeGrants();
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
    TWeakObjectPtr<UGamePlatformEquipmentServerComponent> WeakThis(this);

    const bool bStarted =
        Persistence->BeginLoadEquipment(
            PlayerId,
            CharacterId,
            [WeakThis, ExpectedLifetimeGeneration](
                FGamePlatformEquipmentSnapshot Loaded,
                EGamePlatformEquipmentError Error)
            {
                if (UGamePlatformEquipmentServerComponent* Self =
                    WeakThis.Get())
                {
                    if (Self->bRuntimeClosed || Self->LifetimeGeneration != ExpectedLifetimeGeneration) { return; }
                    Self->HandleLoadCompleted(
                        MoveTemp(Loaded),
                        Error);
                }
            });

    if (!bStarted)
    {
        bPersistenceInFlight = false;
        return false;
    }

    return true;
}

void UGamePlatformEquipmentServerComponent::RegisterDefinition(
    UGamePlatformEquipmentDefinition* Definition)
{
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

    if (AbilitySystem.Get() != InAbilitySystem ||
        AvatarGeneration != InAvatarGeneration)
    {
        RevokeAllRuntimeGrants();
    }

    AbilitySystem = InAbilitySystem;
    AvatarGeneration = FMath::Max(0, InAvatarGeneration);

    if (Snapshot.EquipmentRevision <= 0)
    {
        return true;
    }

    if (!ApplyRuntimeSnapshot(Snapshot))
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
    TWeakObjectPtr<UGamePlatformEquipmentServerComponent> WeakThis(this);

    const bool bStarted =
        Persistence->BeginEquip(
            PlayerId,
            Request,
            [WeakThis, ExpectedLifetimeGeneration, OperationId = Request.OperationId](
                FGamePlatformEquipmentSnapshot Persisted,
                EGamePlatformEquipmentError Error)
            {
                if (UGamePlatformEquipmentServerComponent* Self =
                    WeakThis.Get())
                {
                    if (Self->bRuntimeClosed || Self->LifetimeGeneration != ExpectedLifetimeGeneration) { return; }
                    Self->HandleMutationCompleted(
                        OperationId,
                        MoveTemp(Persisted),
                        Error);
                }
            });

    if (!bStarted)
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
    TWeakObjectPtr<UGamePlatformEquipmentServerComponent> WeakThis(this);

    const bool bStarted =
        Persistence->BeginUnequip(
            PlayerId,
            Request,
            [WeakThis, ExpectedLifetimeGeneration, OperationId = Request.OperationId](
                FGamePlatformEquipmentSnapshot Persisted,
                EGamePlatformEquipmentError Error)
            {
                if (UGamePlatformEquipmentServerComponent* Self =
                    WeakThis.Get())
                {
                    if (Self->bRuntimeClosed || Self->LifetimeGeneration != ExpectedLifetimeGeneration) { return; }
                    Self->HandleMutationCompleted(
                        OperationId,
                        MoveTemp(Persisted),
                        Error);
                }
            });

    if (!bStarted)
    {
        bPersistenceInFlight = false;
        return EGamePlatformEquipmentError::PersistenceUnavailable;
    }

    return EGamePlatformEquipmentError::None;
}

bool UGamePlatformEquipmentServerComponent::Reconcile()
{
    if (bRuntimeClosed || !Persistence.IsValid() ||
        PlayerId.IsEmpty() ||
        CharacterId.IsEmpty() ||
        bPersistenceInFlight)
    {
        return false;
    }

    bPersistenceInFlight = true;
    const uint64 ExpectedLifetimeGeneration = LifetimeGeneration;
    TWeakObjectPtr<UGamePlatformEquipmentServerComponent> WeakThis(this);

    const bool bStarted =
        Persistence->BeginLoadEquipment(
            PlayerId,
            CharacterId,
            [WeakThis, ExpectedLifetimeGeneration](
                FGamePlatformEquipmentSnapshot Loaded,
                EGamePlatformEquipmentError Error)
            {
                if (UGamePlatformEquipmentServerComponent* Self =
                    WeakThis.Get())
                {
                    if (Self->bRuntimeClosed || Self->LifetimeGeneration != ExpectedLifetimeGeneration) { return; }
                    Self->HandleLoadCompleted(
                        MoveTemp(Loaded),
                        Error);
                }
            });

    if (!bStarted)
    {
        bPersistenceInFlight = false;
    }

    return bStarted;
}

void UGamePlatformEquipmentServerComponent::HandleLoadCompleted(
    FGamePlatformEquipmentSnapshot Loaded,
    EGamePlatformEquipmentError Error)
{
    bPersistenceInFlight = false;

    if (Error != EGamePlatformEquipmentError::None ||
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

    if (!ApplyRuntimeSnapshot(Snapshot))
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

    if (Persisted.CharacterId != CharacterId ||
        Persisted.EquipmentRevision <= Snapshot.EquipmentRevision)
    {
        bRuntimeError = true;
        bReady = false;
        LastError = EGamePlatformEquipmentError::InvalidResponse;
        Reconcile();
        return;
    }

    Snapshot = Persisted;

    if (!ApplyRuntimeSnapshot(Persisted))
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


bool UGamePlatformEquipmentServerComponent::ApplyRuntimeSnapshot(
    const FGamePlatformEquipmentSnapshot& Persisted)
{
    UAbilitySystemComponent* ASC = AbilitySystem.Get();

    if (!IsValid(ASC) || !GrantPort.IsValid())
    {
        return false;
    }

    RevokeAllRuntimeGrants();

    TMap<FName, FGamePlatformEquipmentGameplayGrantHandle> NewHandles;

    for (const FGamePlatformEquipmentSlotState& Slot :
         Persisted.Slots)
    {
        UGamePlatformEquipmentDefinition* Definition =
            Definitions.FindRef(Slot.EquipmentDefinitionId);

        if (!IsValid(Definition) ||
            !Definition->IsStructurallyValid() ||
            !Definition->SupportsSlot(Slot.SlotId) ||
            Definition->CompatibleItemDefinitionId !=
                Slot.ItemDefinitionId ||
            Definition->VisualDefinitionId !=
                Slot.VisualDefinitionId)
        {
            for (TPair<
                     FName,
                     FGamePlatformEquipmentGameplayGrantHandle>& Pair :
                 NewHandles)
            {
                GrantPort->Revoke(ASC, Pair.Value);
            }
            return false;
        }

        FGamePlatformEquipmentGameplayGrantHandle Handle;
        if (!GrantPort->Grant(
                ASC,
                *Definition,
                Handle))
        {
            for (TPair<
                     FName,
                     FGamePlatformEquipmentGameplayGrantHandle>& Pair :
                 NewHandles)
            {
                GrantPort->Revoke(ASC, Pair.Value);
            }
            return false;
        }

        NewHandles.Add(Slot.SlotId, MoveTemp(Handle));
    }

    SlotGrantHandles = MoveTemp(NewHandles);
    ++EquipmentRuntimeGeneration;
    return true;
}

void UGamePlatformEquipmentServerComponent::RevokeAllRuntimeGrants()
{
    UAbilitySystemComponent* ASC = AbilitySystem.Get();

    if (GrantPort.IsValid())
    {
        for (TPair<
                 FName,
                 FGamePlatformEquipmentGameplayGrantHandle>& Pair :
             SlotGrantHandles)
        {
            GrantPort->Revoke(ASC, Pair.Value);
        }
    }

    SlotGrantHandles.Reset();
}

void UGamePlatformEquipmentServerComponent::PublishSnapshot()
{
    if (UGamePlatformEquipmentComponent* State =
        StateComponent.Get())
    {
        State->SetServerSnapshots(
            Snapshot,
            EquipmentRuntimeGeneration);
    }
}

// 神兽联盟角色初始化适配：双端游戏线程消费可信身份；本组件拥有Data租约与ASC Gate注册，退出/换身份撤销。
#include "Components/DivineBeastsCharacterComponent.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Abilities/DivineBeastsCharacterActivationGate.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

UDivineBeastsCharacterComponent::UDivineBeastsCharacterComponent()
{
    SetIsReplicatedByDefault(true);
    PrimaryComponentTick.bCanEverTick = false;
}

void UDivineBeastsCharacterComponent::BeginPlay()
{
    Super::BeginPlay();
    bEndingPlay = false;
    RefreshActivationGateBinding();
    RefreshInitialization();
}

void UDivineBeastsCharacterComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
    bEndingPlay = true;
    DetachActivationGate();
    CancelDefinitionLease();
    LoadedDefinition = nullptr;
    bLocalReady = false;
    bConfigurationApplied = false;
    Super::EndPlay(EndPlayReason);
}

void UDivineBeastsCharacterComponent::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(
        UDivineBeastsCharacterComponent,
        CharacterId,
        COND_OwnerOnly);
    DOREPLIFETIME(
        UDivineBeastsCharacterComponent,
        RuntimeState);
    DOREPLIFETIME_CONDITION(
        UDivineBeastsCharacterComponent,
        bPersistentCharacterIdRequired,
        COND_OwnerOnly);
    DOREPLIFETIME(
        UDivineBeastsCharacterComponent,
        bServerReady);
}

bool UDivineBeastsCharacterComponent::AuthorityBindTrustedContext(
    const FGamePlatformCharacterInitializationContext& Context,
    FString& OutError)
{
    check(IsInGameThread());
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!IsValid(Owner) || !Owner->HasAuthority() || bEndingPlay || !World || World->bIsTearingDown)
    {
        OutError = TEXT("AuthorityBindTrustedContext只能由尚未退出的服务器权威世界调用。");
        return false;
    }
    // 监听者可以同步发起下一次绑定，不能跨广播继续借用调用方可修改的Context引用。
    const FGamePlatformCharacterInitializationContext TrustedContext = Context;
    if (!TrustedContext.IsValid(OutError))
    {
        return false;
    }

    EDivineBeastsZodiacIdentity NewZodiac =
        EDivineBeastsZodiacIdentity::Rat;
    if (!FDivineBeastsHeroCatalog::TryGetZodiacIdentity(
            TrustedContext.HeroDefinitionId,
            NewZodiac))
    {
        OutError = TEXT("HeroDefinitionId不在十二生肖核心Catalog。");
        return false;
    }

    if (RuntimeState.SpawnGeneration > 0 &&
        TrustedContext.SpawnGeneration < RuntimeState.SpawnGeneration)
    {
        OutError = TEXT("拒绝旧SpawnGeneration覆盖当前角色。");
        return false;
    }
    if (RuntimeState.AvatarGeneration > 0 &&
        TrustedContext.AvatarGeneration < RuntimeState.AvatarGeneration)
    {
        OutError = TEXT("拒绝旧AvatarGeneration覆盖当前角色。");
        return false;
    }

    const FGuid OperationId = FGuid::NewGuid(); TrustedContextOperationId = OperationId;
    const TWeakObjectPtr<AActor> ExpectedOwner(Owner); const TWeakObjectPtr<UWorld> ExpectedWorld(World);
    const auto IsOriginalOperationCurrent = [this, OperationId, ExpectedOwner, ExpectedWorld]()
    {
        return IsValid(this) && !bEndingPlay && TrustedContextOperationId == OperationId &&
            ExpectedOwner.IsValid() && GetOwner() == ExpectedOwner.Get() && ExpectedOwner->HasAuthority() &&
            ExpectedWorld.IsValid() && GetWorld() == ExpectedWorld.Get() && !ExpectedWorld->bIsTearingDown;
    };
    LoadedDefinition = nullptr;
    bConfigurationApplied = false;
    bLocalReady = false;
    bServerReady = false;
    CancelDefinitionLease();
    if (!IsOriginalOperationCurrent()) { OutError = TEXT("撤销旧定义期间可信绑定已被后继操作或退出接管。"); return false; }
    CharacterId = TrustedContext.CharacterId;
    RuntimeState.HeroDefinitionId = TrustedContext.HeroDefinitionId;
    RuntimeState.ZodiacIdentity = NewZodiac;
    RuntimeState.SpawnGeneration = TrustedContext.SpawnGeneration;
    RuntimeState.AvatarGeneration = TrustedContext.AvatarGeneration;
    // Definition版本只能由服务器实际加载的资产确定，绑定新身份时先清空，禁止沿用旧Hero版本。
    RuntimeState.DefinitionVersion = 0;
    RuntimeState.ContentRevision.Reset();
    bPersistentCharacterIdRequired =
        TrustedContext.bPersistentCharacterIdRequired;

    // 两分支都要求撤销旧资格/技能。统一为一次通知，先发布未Ready的新身份；返回后不得覆盖监听者的更高代次绑定。
    ReadinessChanged.Broadcast(false);
    if (!IsOriginalOperationCurrent()) { OutError = TEXT("未就绪通知已使原可信绑定被后继操作或退出撤销。"); return false; }
    RefreshInitialization();
    if (!IsOriginalOperationCurrent()) { OutError = TEXT("初始化通知已撤销原可信绑定。"); return false; }
    Owner->ForceNetUpdate();
    return true;
}

bool UDivineBeastsCharacterComponent::HasReadableDefinitionResources() const
{
    const UWorld* World = GetWorld(); UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    if (!Data || !World || World->bIsTearingDown) { return false; }
    if (Data->GetLeaseState(DefinitionLease) == EGamePlatformDataRequestState::Succeeded) { return true; }
    return BorrowedWarmupOwner.IsValid() && BorrowedWarmupOwner->GetWorld() == World &&
        Data->GetLeaseState(BorrowedWarmupLease) == EGamePlatformDataRequestState::Succeeded;
}
bool UDivineBeastsCharacterComponent::IsCharacterReady() const
{
    if (bEndingPlay || !HasReadableDefinitionResources()) { return false; }
    return GetOwner() && GetOwner()->HasAuthority() ? bServerReady : bServerReady && bLocalReady;
}
bool UDivineBeastsCharacterComponent::TryUsePreloadedDefinition(const FGamePlatformDataLease& WarmupLease,
    UObject& WarmupOwner, FString& OutError)
{
    check(IsInGameThread());
    UWorld* World = GetWorld(); UGameInstance* Instance = World ? World->GetGameInstance() : nullptr;
    auto* Data = Instance ? IGamePlatformDataService::Get(*Instance) : nullptr;
    const FSoftObjectPath Path = FDivineBeastsHeroCatalog::GetDefinitionAssetPath(RuntimeState.HeroDefinitionId);
    if (!GetOwner() || !GetOwner()->HasAuthority() || bEndingPlay || !IsIdentityStructurallyValid() ||
        !World || World->bIsTearingDown || !IsValid(&WarmupOwner) || WarmupOwner.GetWorld() != World || !Data ||
        !DefinitionLease.IsValid() || !LastDefinitionLoadResult.IsSuccess() || !WarmupLease.ResourcePaths.Contains(Path) ||
        Data->GetLeaseState(WarmupLease) != EGamePlatformDataRequestState::Succeeded)
    { OutError = TEXT("英雄预热交接缺少当前世界的真实成功租约或自身需求未受理。"); return false; }
    auto* Definition = Cast<UDivineBeastsHeroDefinition>(Path.ResolveObject());
    if (!Definition || Definition->DefinitionId != RuntimeState.HeroDefinitionId)
    { OutError = TEXT("预热资源真实Definition与当前可信Hero身份不一致。"); return false; }
    BorrowedWarmupOwner = &WarmupOwner; BorrowedWarmupLease = WarmupLease;
    HandleDefinitionLoaded(Definition, DefinitionRequestGeneration, RuntimeState.SpawnGeneration, RuntimeState.AvatarGeneration);
    if (!IsCharacterReady()) { OutError = TEXT("预热Definition配置或真实角色组件未Ready。"); return false; }
    OutError.Reset(); return true;
}

void UDivineBeastsCharacterComponent::RefreshInitialization()
{
    check(IsInGameThread());
    if (bEndingPlay) { return; }
    const bool bPreviousReady = IsCharacterReady();

    if (!IsIdentityStructurallyValid())
    {
        bLocalReady = false;
        if (GetOwner() && GetOwner()->HasAuthority())
        {
            bServerReady = false;
        }
        BroadcastReadinessIfChanged(bPreviousReady);
        return;
    }

    if (!LoadedDefinition ||
        LoadedDefinition->DefinitionId != RuntimeState.HeroDefinitionId)
    {
        bLocalReady = false;
        bConfigurationApplied = false;
        BeginDefinitionLoad();
        BroadcastReadinessIfChanged(bPreviousReady);
        return;
    }

    FString Error;
    // 同一身份/版本已配置后不重复初始化Momentum；预热交接后的自身租约回调不会重置已参与玩法的GAS属性。
    if (!bConfigurationApplied) { bConfigurationApplied = ApplyDefinition(*LoadedDefinition, Error); }
    bLocalReady = bConfigurationApplied;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bServerReady = bConfigurationApplied;
    }

    BroadcastReadinessIfChanged(bPreviousReady);
}

void UDivineBeastsCharacterComponent::OnRep_RuntimeState()
{
    // RuntimeState是一个原子复制事实。任何变化都作废旧租约，避免旧Hero异步加载完成后阻塞或污染新Hero。
    CancelDefinitionLease();
    LoadedDefinition = nullptr;
    bConfigurationApplied = false;
    bLocalReady = false;
    RefreshInitialization();
}

void UDivineBeastsCharacterComponent::OnRep_ServerReady()
{
    // RepNotify本身即表示服务器Ready事实发生变化；客户端广播当前合成Ready状态。
    RefreshActivationGateBinding();
    ReadinessChanged.Broadcast(IsCharacterReady());
}

void UDivineBeastsCharacterComponent::BeginDefinitionLoad()
{
    if (DefinitionLease.IsValid() || RuntimeState.HeroDefinitionId.IsNone())
    {
        return;
    }

    const int32 RequestGeneration = ++DefinitionRequestGeneration;
    const int32 ExpectedSpawnGeneration = RuntimeState.SpawnGeneration;
    const int32 ExpectedAvatarGeneration = RuntimeState.AvatarGeneration;
    const TWeakObjectPtr<UDivineBeastsCharacterComponent> WeakThis(this);

    UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    if (!Instance)
    {
        LastDefinitionLoadResult = FGamePlatformResult::Failure(TEXT("DataGameInstanceUnavailable"), TEXT("角色所在世界没有资源租约作用域。"));
        UpdateReadiness();
        return;
    }
    DefinitionLease = FDivineBeastsHeroCatalog::AcquireDefinitionResources(
        *Instance, RuntimeState.HeroDefinitionId, EGamePlatformDataLifetime::World, this,
        [WeakThis, RequestGeneration, ExpectedSpawnGeneration, ExpectedAvatarGeneration](
            UDivineBeastsHeroDefinition* Definition, const FGamePlatformDataLease& CompletedLease, const FGamePlatformResult& Result)
        {
            if (!WeakThis.IsValid() || WeakThis->bEndingPlay ||
                RequestGeneration != WeakThis->DefinitionRequestGeneration ||
                CompletedLease.ScopeId != WeakThis->DefinitionLease.ScopeId ||
                CompletedLease.LeaseId != WeakThis->DefinitionLease.LeaseId ||
                CompletedLease.Generation != WeakThis->DefinitionLease.Generation)
            { return; }
            WeakThis->LastDefinitionLoadResult = Result;
            if (Result.IsSuccess()) { WeakThis->BorrowedWarmupOwner.Reset(); WeakThis->BorrowedWarmupLease = {}; }
            // 完成只是加载终态；成功租约必须继续覆盖角色使用期，失败仅撤销本组件需求。
            if (!Result.IsSuccess()) { WeakThis->CancelDefinitionLease(); }
            WeakThis->HandleDefinitionLoaded(Result.IsSuccess() ? Definition : nullptr,
                WeakThis->DefinitionRequestGeneration, ExpectedSpawnGeneration, ExpectedAvatarGeneration);
        }, LastDefinitionLoadResult);
    if (!LastDefinitionLoadResult.IsSuccess())
    {
        LoadedDefinition = nullptr;
        bConfigurationApplied = false;
        UpdateReadiness();
    }
}

void UDivineBeastsCharacterComponent::CancelDefinitionLease()
{
    ++DefinitionRequestGeneration;
    // 借用只撤销本组件引用，不释放装配拥有的预热需求。
    BorrowedWarmupOwner.Reset(); BorrowedWarmupLease = {};
    // 先取走本次租约，再调用外部服务；同步后继绑定可以安装新租约，旧取消栈不能在返回后清空它。
    const FGamePlatformDataLease OwnedLease = DefinitionLease; DefinitionLease = {};
    if (OwnedLease.IsValid())
    {
        if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
        {
            if (auto* Data = IGamePlatformDataService::Get(*Instance)) { Data->ReleaseResources(OwnedLease); }
        }
    }
}

void UDivineBeastsCharacterComponent::HandleDefinitionLoaded(
    UDivineBeastsHeroDefinition* Definition,
    int32 ExpectedRequestGeneration,
    int32 ExpectedSpawnGeneration,
    int32 ExpectedAvatarGeneration)
{
    if (ExpectedRequestGeneration != DefinitionRequestGeneration ||
        ExpectedSpawnGeneration != RuntimeState.SpawnGeneration ||
        ExpectedAvatarGeneration != RuntimeState.AvatarGeneration)
    {
        return;
    }

    if (!Definition || Definition->DefinitionId != RuntimeState.HeroDefinitionId)
    {
        if (Definition) { LastDefinitionLoadResult = FGamePlatformResult::Failure(TEXT("HeroDefinitionMismatch"), TEXT("真实Definition与当前可信Hero身份不一致。")); }
        CancelDefinitionLease();
        LoadedDefinition = nullptr;
        bLocalReady = false;
        bConfigurationApplied = false;
        UpdateReadiness();
        // Data已进入失败/取消终态，旧Ready查询已为false；仍显式通知宿主撤销此前公布的Active。
        ReadinessChanged.Broadcast(false);
        return;
    }

    // 服务器以实际加载资产为唯一版本事实，再复制给客户端做内容一致性门禁。
    if (GetOwner() && GetOwner()->HasAuthority())
    {
        RuntimeState.DefinitionVersion = Definition->Version;
        RuntimeState.ContentRevision = Definition->ContentRevision;
        GetOwner()->ForceNetUpdate();
    }

    LoadedDefinition = Definition;
    RefreshInitialization();
}

bool UDivineBeastsCharacterComponent::ApplyDefinition(
    const UDivineBeastsHeroDefinition& Definition,
    FString& OutError)
{
    if (!Definition.IsProjectDefinitionValid(OutError))
    {
        return false;
    }

    // 两端都必须以服务器认可的版本事实作为Ready门禁。这样客户端内容包落后或服务器资产未完成确认时不会静默进入玩法。
    if (RuntimeState.DefinitionVersion <= 0 ||
        RuntimeState.ContentRevision.TrimStartAndEnd().IsEmpty())
    {
        OutError = TEXT("服务器尚未发布Hero Definition版本事实。");
        return false;
    }
    if (Definition.Version != RuntimeState.DefinitionVersion ||
        Definition.ContentRevision != RuntimeState.ContentRevision)
    {
        OutError = TEXT("本地Hero Definition版本与服务器认可版本不一致。");
        return false;
    }

    ACharacter* Character = Cast<ACharacter>(GetOwner());
    if (!Character)
    {
        OutError = TEXT("DivineBeastsCharacterComponent必须挂在ACharacter上。");
        return false;
    }

    UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
    UCharacterMovementComponent* Movement =
        Character->GetCharacterMovement();
    if (!Capsule || !Movement)
    {
        OutError = TEXT("ACharacter缺少Capsule或CharacterMovement组件。");
        return false;
    }
    if (!Character->HasActorBegunPlay() ||
        !Capsule->IsRegistered() ||
        !Movement->IsRegistered())
    {
        OutError = TEXT("平台ACharacter基础生命周期/移动/碰撞组件尚未就绪。");
        return false;
    }

    auto* AbilitySystem = Character->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    auto* Combat = Character->FindComponentByClass<UGamePlatformCombatComponent>();
    if (!AbilitySystem || !Combat || AbilitySystem->GetAvatarActor() != Character ||
        !AbilitySystem->GetAvatarBindingSnapshot().bBound)
    { OutError = TEXT("角色缺少ASC、Combat或本Avatar的ActorInfo，禁止发布Ready。"); return false; }

    Capsule->SetCapsuleSize(
        Definition.SpawnEnvelope.CapsuleRadius,
        Definition.SpawnEnvelope.CapsuleHalfHeight,
        true);
    Capsule->SetCollisionProfileName(
        Definition.SpawnEnvelope.CollisionProfileName);

    Movement->MaxWalkSpeed = Definition.Movement.MaxWalkSpeed;
    Movement->MaxAcceleration = Definition.Movement.MaxAcceleration;
    Movement->JumpZVelocity = Definition.Movement.JumpZVelocity;
    Movement->RotationRate.Yaw = Definition.Movement.RotationRateYaw;
    Movement->NavAgentProps.bCanCrouch = Definition.Movement.bCanCrouch;

    // Momentum（气势）是神兽联盟项目层核心状态。只在服务器确保 AttributeSet 存在并按 Hero Definition 初始化；
    // 客户端通过 GAS 复制接收，不在 CharacterComponent/UI 保存第二份权威真值。
    if (Character->HasAuthority())
    {
        // 新Pawn的Combat初始代次为1；绑定更大可信Avatar代次时只重置一次，不能让同代次重复配置递增到另一身份。
        if (!Combat->GetCombatAttributeSet() || Combat->GetCombatAvatarGeneration() > RuntimeState.AvatarGeneration ||
            (Combat->GetCombatAvatarGeneration() < RuntimeState.AvatarGeneration && !Combat->ResetForNewAvatar(RuntimeState.AvatarGeneration)))
        { OutError = TEXT("战斗组件未建立同Avatar代次的权威属性状态。"); return false; }
        if (UGamePlatformAbilitySystemComponent* MomentumAbilitySystem =
                Character->FindComponentByClass<UGamePlatformAbilitySystemComponent>())
        {
            UDivineBeastsMomentumAttributeSet* MomentumAttributes =
                const_cast<UDivineBeastsMomentumAttributeSet*>(
                    MomentumAbilitySystem->GetSet<UDivineBeastsMomentumAttributeSet>());
            if (!IsValid(MomentumAttributes))
            {
                MomentumAttributes = const_cast<UDivineBeastsMomentumAttributeSet*>(
                    MomentumAbilitySystem->AddSet<UDivineBeastsMomentumAttributeSet>());
            }
            if (!IsValid(MomentumAttributes))
            { OutError = TEXT("服务器未建立Momentum属性集，禁止发布Ready。"); return false; }
            MomentumAttributes->InitializeFromDefinition(Definition.Momentum);
        }
    }

    return true;
}

bool UDivineBeastsCharacterComponent::IsIdentityStructurallyValid() const
{
    if (!FDivineBeastsHeroCatalog::IsCoreHeroId(RuntimeState.HeroDefinitionId) ||
        RuntimeState.SpawnGeneration <= 0 ||
        RuntimeState.AvatarGeneration <= 0)
    {
        return false;
    }

    // CharacterId只对服务器和拥有者有意义；远端观察者不应因隐私最小化而无法初始化Hero外观/碰撞。
    if (GetOwner() && GetOwner()->HasAuthority() &&
        bPersistentCharacterIdRequired && CharacterId.IsEmpty())
    {
        return false;
    }

    EDivineBeastsZodiacIdentity Expected =
        EDivineBeastsZodiacIdentity::Rat;
    return FDivineBeastsHeroCatalog::TryGetZodiacIdentity(
            RuntimeState.HeroDefinitionId,
            Expected)
        && Expected == RuntimeState.ZodiacIdentity;
}

void UDivineBeastsCharacterComponent::UpdateReadiness()
{
    const bool bPreviousReady = IsCharacterReady();
    bLocalReady = IsIdentityStructurallyValid()
        && LoadedDefinition != nullptr
        && bConfigurationApplied;

    if (GetOwner() && GetOwner()->HasAuthority())
    {
        bServerReady = bLocalReady;
    }
    BroadcastReadinessIfChanged(bPreviousReady);
}

void UDivineBeastsCharacterComponent::BroadcastReadinessIfChanged(
    bool bPreviousReady)
{
    RefreshActivationGateBinding();
    const bool bCurrentReady = IsCharacterReady();
    if (bPreviousReady != bCurrentReady)
    {
        ReadinessChanged.Broadcast(bCurrentReady);
    }
}

void UDivineBeastsCharacterComponent::RefreshActivationGateBinding()
{
    check(IsInGameThread());
    if (bEndingPlay) { return; }
    AActor* Owner = GetOwner();
    UGamePlatformAbilitySystemComponent* CurrentASC = Owner ? Owner->FindComponentByClass<UGamePlatformAbilitySystemComponent>() : nullptr;
    // 支持宿主把ASC放在PlayerState/Controller；资格仍核对其ActorInfo当前Avatar和真实所有权。
    if (!CurrentASC)
    {
        if (const auto* Pawn = Cast<APawn>(Owner))
        {
            const auto* Controller = Cast<APlayerController>(Pawn->GetController());
            if (Controller)
            {
                CurrentASC = Controller->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
                if (!CurrentASC && Controller->PlayerState)
                { CurrentASC = Controller->PlayerState->FindComponentByClass<UGamePlatformAbilitySystemComponent>(); }
            }
        }
    }
    if (BoundAbilitySystem.Get() != CurrentASC)
    {
        DetachActivationGate();
        BoundAbilitySystem = CurrentASC;
        if (CurrentASC)
        {
            AvatarBindingChangedHandle = CurrentASC->OnAvatarBindingChanged().AddUObject(this,
                &UDivineBeastsCharacterComponent::HandleAbilityAvatarBindingChanged);
        }
    }
    if (!CurrentASC) { return; }
    CurrentASC->ClearActivationGate(this);
    const auto Snapshot = CurrentASC->GetAvatarBindingSnapshot();
    const APawn* PawnOwner = Cast<APawn>(Owner);
    // 玩家项目Gate不能抢占AI/NPC控制器的中立Gate；无Controller时先保持失败关闭，随后ActorInfo事件重评估。
    const bool bPlayerOrUnpossessed = !PawnOwner || !PawnOwner->GetController() || Cast<APlayerController>(PawnOwner->GetController());
    if (bPlayerOrUnpossessed && Snapshot.bBound && CurrentASC->GetAvatarActor() == Owner)
    {
        // 即使尚未Ready也注入只读Gate，Evaluate返回明确CharacterNotReady；Ready/身份变化会重新捕获代次。
        CurrentASC->SetActivationGate(this, MakeShared<FDivineBeastsCharacterActivationGate>(*this,
            RuntimeState.SpawnGeneration, RuntimeState.AvatarGeneration));
    }
}

void UDivineBeastsCharacterComponent::HandleAbilityAvatarBindingChanged(const FGamePlatformAbilityAvatarBindingSnapshot& Snapshot)
{
    (void)Snapshot;
    // ASC已经在新ActorInfo就绪后发布事件；旧Gate已由ASC撤销，再按当前项目身份重新注入。
    RefreshActivationGateBinding();
}

void UDivineBeastsCharacterComponent::DetachActivationGate()
{
    if (auto* ASC = BoundAbilitySystem.Get())
    {
        ASC->ClearActivationGate(this);
        ASC->OnAvatarBindingChanged().Remove(AvatarBindingChangedHandle);
    }
    AvatarBindingChangedHandle.Reset();
    BoundAbilitySystem.Reset();
}

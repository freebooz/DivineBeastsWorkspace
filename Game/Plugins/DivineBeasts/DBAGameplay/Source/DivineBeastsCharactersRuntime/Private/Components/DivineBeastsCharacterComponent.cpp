#include "Components/DivineBeastsCharacterComponent.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Attributes/DivineBeastsMomentumAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "Engine/StreamableManager.h"
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
    RefreshInitialization();
}

void UDivineBeastsCharacterComponent::EndPlay(
    const EEndPlayReason::Type EndPlayReason)
{
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
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority())
    {
        OutError = TEXT("AuthorityBindTrustedContext只能由服务器权威路径调用。");
        return false;
    }
    if (!Context.IsValid(OutError))
    {
        return false;
    }

    EDivineBeastsZodiacIdentity NewZodiac =
        EDivineBeastsZodiacIdentity::Rat;
    if (!FDivineBeastsHeroCatalog::TryGetZodiacIdentity(
            Context.HeroDefinitionId,
            NewZodiac))
    {
        OutError = TEXT("HeroDefinitionId不在十二生肖核心Catalog。");
        return false;
    }

    if (RuntimeState.SpawnGeneration > 0 &&
        Context.SpawnGeneration < RuntimeState.SpawnGeneration)
    {
        OutError = TEXT("拒绝旧SpawnGeneration覆盖当前角色。");
        return false;
    }
    if (RuntimeState.AvatarGeneration > 0 &&
        Context.AvatarGeneration < RuntimeState.AvatarGeneration)
    {
        OutError = TEXT("拒绝旧AvatarGeneration覆盖当前角色。");
        return false;
    }

    // 新可信出生身份（即使上一个英雄尚在异步加载）必须先作废旧技能授权。
    // 此时角色和技能装配组件仍在同一服务器实例，立即广播未就绪，
    // 避免先清除 bServerReady 再刷新导致漏掉 Ready->NotReady 边沿，残留旧技能。
    // 首次绑定也允许无害广播 false：调用者据此取消任何本代次以前的技能租约。
    ReadinessChanged.Broadcast(false);

    CancelDefinitionLease();
    LoadedDefinition = nullptr;
    bConfigurationApplied = false;
    bLocalReady = false;
    bServerReady = false;

    CharacterId = Context.CharacterId;
    RuntimeState.HeroDefinitionId = Context.HeroDefinitionId;
    RuntimeState.ZodiacIdentity = NewZodiac;
    RuntimeState.SpawnGeneration = Context.SpawnGeneration;
    RuntimeState.AvatarGeneration = Context.AvatarGeneration;
    // Definition版本只能由服务器实际加载的资产确定，绑定新身份时先清空，禁止沿用旧Hero版本。
    RuntimeState.DefinitionVersion = 0;
    RuntimeState.ContentRevision.Reset();
    bPersistentCharacterIdRequired =
        Context.bPersistentCharacterIdRequired;

    RefreshInitialization();
    Owner->ForceNetUpdate();
    return true;
}

void UDivineBeastsCharacterComponent::RefreshInitialization()
{
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
    bConfigurationApplied = ApplyDefinition(*LoadedDefinition, Error);
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

    DefinitionLease = FDivineBeastsHeroCatalog::RequestDefinition(
        RuntimeState.HeroDefinitionId,
        [WeakThis,
         RequestGeneration,
         ExpectedSpawnGeneration,
         ExpectedAvatarGeneration](
            UDivineBeastsHeroDefinition* Definition)
        {
            if (WeakThis.IsValid())
            {
                WeakThis->HandleDefinitionLoaded(
                    Definition,
                    RequestGeneration,
                    ExpectedSpawnGeneration,
                    ExpectedAvatarGeneration);
            }
        });
}

void UDivineBeastsCharacterComponent::CancelDefinitionLease()
{
    ++DefinitionRequestGeneration;
    if (DefinitionLease.IsValid())
    {
        DefinitionLease->CancelHandle();
        DefinitionLease.Reset();
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

    DefinitionLease.Reset();
    if (!Definition || Definition->DefinitionId != RuntimeState.HeroDefinitionId)
    {
        LoadedDefinition = nullptr;
        bLocalReady = false;
        bConfigurationApplied = false;
        UpdateReadiness();
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
        if (UGamePlatformAbilitySystemComponent* AbilitySystem =
                Character->FindComponentByClass<UGamePlatformAbilitySystemComponent>())
        {
            UDivineBeastsMomentumAttributeSet* MomentumAttributes =
                const_cast<UDivineBeastsMomentumAttributeSet*>(
                    AbilitySystem->GetSet<UDivineBeastsMomentumAttributeSet>());
            if (!IsValid(MomentumAttributes))
            {
                MomentumAttributes = const_cast<UDivineBeastsMomentumAttributeSet*>(
                    AbilitySystem->AddSet<UDivineBeastsMomentumAttributeSet>());
            }
            if (IsValid(MomentumAttributes))
            {
                MomentumAttributes->InitializeFromDefinition(Definition.Momentum);
            }
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
    const bool bCurrentReady = IsCharacterReady();
    if (bPreviousReady != bCurrentReady)
    {
        ReadinessChanged.Broadcast(bCurrentReady);
    }
}

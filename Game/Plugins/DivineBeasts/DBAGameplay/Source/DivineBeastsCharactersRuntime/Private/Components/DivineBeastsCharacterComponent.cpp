#include "Components/DivineBeastsCharacterComponent.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
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
        HeroDefinitionId);
    DOREPLIFETIME(
        UDivineBeastsCharacterComponent,
        ZodiacIdentity);
    DOREPLIFETIME(
        UDivineBeastsCharacterComponent,
        SpawnGeneration);
    DOREPLIFETIME(
        UDivineBeastsCharacterComponent,
        AvatarGeneration);
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

    if (SpawnGeneration > 0 &&
        Context.SpawnGeneration < SpawnGeneration)
    {
        OutError = TEXT("拒绝旧SpawnGeneration覆盖当前角色。");
        return false;
    }
    if (AvatarGeneration > 0 &&
        Context.AvatarGeneration < AvatarGeneration)
    {
        OutError = TEXT("拒绝旧AvatarGeneration覆盖当前角色。");
        return false;
    }

    CancelDefinitionLease();
    LoadedDefinition = nullptr;
    bConfigurationApplied = false;
    bLocalReady = false;
    bServerReady = false;

    CharacterId = Context.CharacterId;
    HeroDefinitionId = Context.HeroDefinitionId;
    ZodiacIdentity = NewZodiac;
    SpawnGeneration = Context.SpawnGeneration;
    AvatarGeneration = Context.AvatarGeneration;
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
        LoadedDefinition->DefinitionId != HeroDefinitionId)
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

void UDivineBeastsCharacterComponent::OnRep_Identity()
{
    RefreshInitialization();
}

void UDivineBeastsCharacterComponent::OnRep_Generation()
{
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
    if (DefinitionLease.IsValid() || HeroDefinitionId.IsNone())
    {
        return;
    }

    const int32 RequestGeneration = ++DefinitionRequestGeneration;
    const int32 ExpectedSpawnGeneration = SpawnGeneration;
    const int32 ExpectedAvatarGeneration = AvatarGeneration;
    const TWeakObjectPtr<UDivineBeastsCharacterComponent> WeakThis(this);

    DefinitionLease = FDivineBeastsHeroCatalog::RequestDefinition(
        HeroDefinitionId,
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
        ExpectedSpawnGeneration != SpawnGeneration ||
        ExpectedAvatarGeneration != AvatarGeneration)
    {
        return;
    }

    DefinitionLease.Reset();
    if (!Definition || Definition->DefinitionId != HeroDefinitionId)
    {
        LoadedDefinition = nullptr;
        bLocalReady = false;
        bConfigurationApplied = false;
        UpdateReadiness();
        return;
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

    return true;
}

bool UDivineBeastsCharacterComponent::IsIdentityStructurallyValid() const
{
    if (!FDivineBeastsHeroCatalog::IsCoreHeroId(HeroDefinitionId) ||
        SpawnGeneration <= 0 ||
        AvatarGeneration <= 0)
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
            HeroDefinitionId,
            Expected)
        && Expected == ZodiacIdentity;
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

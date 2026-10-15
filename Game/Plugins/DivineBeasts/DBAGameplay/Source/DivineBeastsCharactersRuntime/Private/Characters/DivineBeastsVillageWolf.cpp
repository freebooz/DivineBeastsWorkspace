// SSLM项目层狼怪物生命适配：保持与开发石灵独立，绝不让石灵获得攻击能力。
#include "Characters/DivineBeastsVillageWolf.h"

#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformAIStateComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Types/GamePlatformCombatEvent.h"

ADivineBeastsVillageWolf::ADivineBeastsVillageWolf()
{
    bReplicates = true;
    SetReplicateMovement(true);
    PrimaryActorTick.bCanEverTick = false;
    AutoPossessAI = EAutoPossessAI::Disabled;
    AutoPossessPlayer = EAutoReceiveInput::Disabled;
    AIState = CreateDefaultSubobject<UGamePlatformAIStateComponent>(TEXT("VillageWolfAIState"));
    GetCapsuleComponent()->InitCapsuleSize(55.0f, 85.0f);
    GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
    GetCharacterMovement()->MaxWalkSpeed = 375.0f;
    GetCharacterMovement()->bOrientRotationToMovement = true;
    bUseControllerRotationYaw = false;
    // 服务器不烘焙/加载Fab纯美术；初始Mesh为空由项目客户端资源服务补齐。
    GetGamePlatformAbilitySystemComponent()->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void ADivineBeastsVillageWolf::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
        DeathDelegate = OnAuthoritativeDeath().AddUObject(this, &ThisClass::HandleAuthorityDeath);
}

bool ADivineBeastsVillageWolf::AuthorityInitializeWolf(float MaxHealth)
{
    auto* ASC = GetGamePlatformAbilitySystemComponent();
    auto* Combat = GetGamePlatformCombatComponent();
    UWorld* World = GetWorld();
    if (!IsInGameThread() || !HasAuthority() || bClosing || bWolfInitialized ||
        IsActorBeingDestroyed() || !HasActorBegunPlay() || !IsValid(World) || World->bIsTearingDown ||
        !GetController() || !FMath::IsFinite(MaxHealth) || MaxHealth <= 0.0f || MaxHealth > 100000.0f ||
        !IsValid(ASC) || !IsValid(Combat) || !Combat->HasBegunPlay() ||
        !Combat->GetCombatAttributeSet()) return false;

    // PossessedBy已建立唯一ASC ActorInfo；允许首次没有绑定时按NPC身份修复，不使用玩家Ready。
    if ((ASC->GetOwnerActor() != this || ASC->GetAvatarActor() != this) &&
        !ASC->BindAbilityActorInfo(this, this)) return false;
    if (ASC->GetOwnerActor() != this || ASC->GetAvatarActor() != this) return false;

    const int32 Before = Combat->GetCombatAvatarGeneration();
    if (Before <= 0 || Before == MAX_int32) return false;
    ASC->SetNumericAttributeBase(UGamePlatformCombatAttributeSet::GetMaxHealthAttribute(), MaxHealth);
    if (!IsValid(this) || bClosing || IsActorBeingDestroyed() || !IsValid(Combat) ||
        Combat->GetCombatAvatarGeneration() != Before) return false;
    const bool bReset = Combat->ResetForNewAvatar(Before + 1, 1.0f);
    if (!IsValid(this) || bClosing || IsActorBeingDestroyed() || !bReset ||
        Combat->GetCombatAvatarGeneration() != Before + 1 || Combat->IsCombatDead() ||
        !FMath::IsNearlyEqual(Combat->GetCombatHealth(), MaxHealth)) return false;
    bWolfDead = false;
    bWolfInitialized = true;
    ForceNetUpdate();
    return true;
}

bool ADivineBeastsVillageWolf::CanSourceCombat() const
{
    const auto* Combat = GetGamePlatformCombatComponent();
    const auto* ASC = GetGamePlatformAbilitySystemComponent();
    const UWorld* World = GetWorld();
    return IsWolfAlive() && !bClosing && !IsActorBeingDestroyed() && World && !World->bIsTearingDown &&
        IsValid(Combat) && Combat->GetCombatHealth() > 0.0f && !Combat->IsCombatDead() &&
        IsValid(ASC) && ASC->GetOwnerActor() == this && ASC->GetAvatarActor() == this;
}

bool ADivineBeastsVillageWolf::CanReceiveCombat() const { return CanSourceCombat(); }

int32 ADivineBeastsVillageWolf::GetCombatAvatarGeneration() const
{
    const auto* Combat = GetGamePlatformCombatComponent();
    return IsValid(Combat) ? Combat->GetCombatAvatarGeneration() : 0;
}

void ADivineBeastsVillageWolf::HandleAuthorityDeath(const FGamePlatformCombatEvent& Event)
{
    auto* Combat = GetGamePlatformCombatComponent();
    if (bClosing || !HasAuthority() || !bWolfInitialized || bWolfDead ||
        Event.TargetActor != this || Event.EventType != EGamePlatformCombatEventType::Death ||
        !IsValid(Combat) || !Combat->IsCombatDead() ||
        Combat->GetCombatAvatarGeneration() != Event.TargetAvatarGeneration) return;
    bWolfDead = true;
    OnRep_WolfDead();
    ForceNetUpdate();
}

void ADivineBeastsVillageWolf::OnRep_WolfDead()
{
    if (bWolfDead)
    {
        GetCharacterMovement()->StopMovementImmediately();
        GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

void ADivineBeastsVillageWolf::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADivineBeastsVillageWolf, bWolfInitialized);
    DOREPLIFETIME(ADivineBeastsVillageWolf, bWolfDead);
}

void ADivineBeastsVillageWolf::EndPlay(const EEndPlayReason::Type Reason)
{
    bClosing = true;
    OnAuthoritativeDeath().Remove(DeathDelegate);
    DeathDelegate.Reset();
    Super::EndPlay(Reason);
}

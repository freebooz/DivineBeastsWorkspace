// 双端项目Pawn生命周期适配：组件是默认子对象，真实网络实体和GAS只维护一套状态；没有客户端权威命令。
#include "Characters/DivineBeastsCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Components/GamePlatformAITargetComponent.h"
#include "GameFramework/Controller.h"
#include "Types/GamePlatformCombatEvent.h"

ADivineBeastsCharacter::ADivineBeastsCharacter()
{
    AbilitySystem = CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilityCast = CreateDefaultSubobject<UGamePlatformAbilityCastComponent>(TEXT("AbilityCast"));
    Combat = CreateDefaultSubobject<UGamePlatformCombatComponent>(TEXT("Combat"));
    GameplayEligibility = CreateDefaultSubobject<UGamePlatformGameplayEligibilityComponent>(TEXT("GameplayEligibility"));
    AITarget = CreateDefaultSubobject<UGamePlatformAITargetComponent>(TEXT("AITarget"));
    CharacterIdentity = CreateDefaultSubobject<UDivineBeastsCharacterComponent>(TEXT("CharacterIdentity"));
}
UAbilitySystemComponent* ADivineBeastsCharacter::GetAbilitySystemComponent() const { return AbilitySystem; }
UGamePlatformAbilitySystemComponent* ADivineBeastsCharacter::GetGamePlatformAbilitySystemComponent() const { return AbilitySystem; }
UGamePlatformCombatComponent* ADivineBeastsCharacter::GetGamePlatformCombatComponent() const { return Combat; }
bool ADivineBeastsCharacter::CanSourceCombat() const
{ return CharacterIdentity->IsCharacterReady() && !Combat->IsCombatDead() && AbilitySystem->EvaluateActivationEligibility().IsSuccess(); }
bool ADivineBeastsCharacter::CanReceiveCombat() const { return CanSourceCombat(); }
int32 ADivineBeastsCharacter::GetCombatAvatarGeneration() const { return CharacterIdentity->GetAvatarGeneration(); }
void ADivineBeastsCharacter::BeginPlay()
{
    Super::BeginPlay();
    Combat->OnCombatEvent.AddDynamic(this, &ADivineBeastsCharacter::HandleCombatEvent);
    // AI资格由角色真实Ready/玩家控制权/死亡事实驱动；不因狼创建而主动扫描所有Pawn。
    if (HasAuthority()) AITarget->SetAITargetEnabled(false);
    CharacterIdentity->OnReadinessChanged().AddWeakLambda(this, [this](bool bReady)
    {
        if (HasAuthority())
            AITarget->SetAITargetEnabled(bReady && GetController() && GetController()->IsPlayerController() && !Combat->IsCombatDead());
        if (!bReady) { GameplayEligibility->SetServerPlayerActive(false);
            AbilityCast->CancelConfirmedCast(AbilityCast->GetCastFacts().ActivationId); }
    });
    RefreshAbilityActorInfo();
    if (HasAuthority())
        AITarget->SetAITargetEnabled(CharacterIdentity->IsCharacterReady() && GetController() &&
            GetController()->IsPlayerController() && !Combat->IsCombatDead());
}
void ADivineBeastsCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    RefreshAbilityActorInfo();
    if (HasAuthority()) AITarget->SetAITargetEnabled(NewController && NewController->IsPlayerController() &&
        CharacterIdentity->IsCharacterReady() && !Combat->IsCombatDead());
}
void ADivineBeastsCharacter::OnRep_Controller()
{ Super::OnRep_Controller(); RefreshAbilityActorInfo(); }
void ADivineBeastsCharacter::UnPossessed()
{
    if (HasAuthority()) AITarget->SetAITargetEnabled(false);
    GameplayEligibility->SetServerPlayerActive(false);
    AbilityCast->CancelConfirmedCast(AbilityCast->GetCastFacts().ActivationId);
    AbilitySystem->ClearAbilityAvatar();
    Super::UnPossessed();
}
void ADivineBeastsCharacter::RefreshAbilityActorInfo()
{
    // 远端客户端Controller不复制给观察者，但仍需ActorInfo观察GAS属性；激活Gate继续拒绝非本地拥有者。
    if (GetController() || !HasAuthority()) { AbilitySystem->BindAbilityActorInfo(this, this); }
    else { GameplayEligibility->SetServerPlayerActive(false); AbilitySystem->ClearAbilityAvatar(); }
}
void ADivineBeastsCharacter::HandleCombatEvent(const FGamePlatformCombatEvent& Event)
{
    // 真实GAS重生通知后恢复玩家目标资格；狼等非PlayerController实体永远不会进入可被追踪玩家集合。
    if (HasAuthority() && Event.TargetActor == this && Event.EventType == EGamePlatformCombatEventType::RespawnReset &&
        Event.TargetAvatarGeneration > 0 && Event.TargetAvatarGeneration == Combat->GetCombatAvatarGeneration())
    {
        AITarget->SetAITargetEnabled(CharacterIdentity->IsCharacterReady() && GetController() &&
            GetController()->IsPlayerController() && !Combat->IsCombatDead());
        return;
    }
    if (!HasAuthority() || Event.EventType != EGamePlatformCombatEventType::Death || Event.TargetActor != this ||
        !Event.EventId.IsValid() || Event.TargetAvatarGeneration <= 0 ||
        Event.TargetAvatarGeneration != Combat->GetCombatAvatarGeneration() || !Combat->IsCombatDead()) { return; }
    AITarget->SetAITargetEnabled(false);
    GameplayEligibility->SetServerPlayerActive(false);
    if (PublishedDeathGeneration == Event.TargetAvatarGeneration) { return; }
    AbilityCast->CancelConfirmedCast(AbilityCast->GetCastFacts().ActivationId);
    PublishedDeathGeneration = Event.TargetAvatarGeneration;
    // 值拷贝隔离同步消费者；击杀方收到转发的同一Combat事件时，Target检查禁止误报自己死亡。
    const FGamePlatformCombatEvent Death = Event;
    AuthoritativeDeath.Broadcast(Death);
}
void ADivineBeastsCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    if (HasAuthority()) AITarget->SetAITargetEnabled(false);
    AuthoritativeDeath.Clear();
    GameplayEligibility->SetServerPlayerActive(false);
    CharacterIdentity->OnReadinessChanged().RemoveAll(this);
    Combat->OnCombatEvent.RemoveDynamic(this, &ADivineBeastsCharacter::HandleCombatEvent);
    AbilitySystem->ClearAbilityAvatar();
    Super::EndPlay(Reason);
}

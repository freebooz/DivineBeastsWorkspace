// 双端项目Pawn生命周期适配：组件是默认子对象，真实网络实体和GAS只维护一套状态；没有客户端权威命令。
#include "Characters/DivineBeastsCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformGameplayEligibilityComponent.h"
#include "Types/GamePlatformCombatEvent.h"

ADivineBeastsCharacter::ADivineBeastsCharacter()
{
    AbilitySystem = CreateDefaultSubobject<UGamePlatformAbilitySystemComponent>(TEXT("AbilitySystem"));
    AbilityCast = CreateDefaultSubobject<UGamePlatformAbilityCastComponent>(TEXT("AbilityCast"));
    Combat = CreateDefaultSubobject<UGamePlatformCombatComponent>(TEXT("Combat"));
    GameplayEligibility = CreateDefaultSubobject<UGamePlatformGameplayEligibilityComponent>(TEXT("GameplayEligibility"));
    CharacterIdentity = CreateDefaultSubobject<UDivineBeastsCharacterComponent>(TEXT("CharacterIdentity"));
}
UAbilitySystemComponent* ADivineBeastsCharacter::GetAbilitySystemComponent() const { return AbilitySystem; }
UGamePlatformAbilitySystemComponent* ADivineBeastsCharacter::GetGamePlatformAbilitySystemComponent() const { return AbilitySystem; }
UGamePlatformCombatComponent* ADivineBeastsCharacter::GetGamePlatformCombatComponent() const { return Combat; }
bool ADivineBeastsCharacter::CanSourceCombat() const
{ return CharacterIdentity->IsCharacterReady() && !Combat->IsCombatDead() && AbilitySystem->EvaluateActivationEligibility().IsSuccess(); }
bool ADivineBeastsCharacter::CanReceiveCombat() const
{ return CanSourceCombat() || (IsDisconnectedCombatTargetRetained() && !Combat->IsCombatDead()); }
bool ADivineBeastsCharacter::IsDisconnectedCombatTargetRetained()const
{
    return HasAuthority() && !GetController() && DisconnectedCombatTargetGeneration>0 &&
        DisconnectedCombatTargetGeneration==GetCombatAvatarGeneration() && CharacterIdentity->IsCharacterReady() &&
        AbilitySystem->GetOwnerActor()==this && AbilitySystem->GetAvatarActor()==this &&
        Combat->GetCombatAvatarGeneration()==DisconnectedCombatTargetGeneration;
}
bool ADivineBeastsCharacter::SetDisconnectedCombatTargetRetained(int32 ExpectedGeneration,bool bRetained)
{
    if(!HasAuthority()||ExpectedGeneration<=0||ExpectedGeneration!=GetCombatAvatarGeneration()||
        Combat->GetCombatAvatarGeneration()!=ExpectedGeneration)return false;
    if(bRetained&&(!CharacterIdentity->IsCharacterReady()||Combat->IsCombatDead()||
        AbilitySystem->GetOwnerActor()!=this||AbilitySystem->GetAvatarActor()!=this))return false;
    DisconnectedCombatTargetGeneration=bRetained?ExpectedGeneration:0;
    if(!bRetained&&!GetController())AbilitySystem->ClearAbilityAvatar();
    return true;
}
int32 ADivineBeastsCharacter::GetCombatAvatarGeneration() const { return CharacterIdentity->GetAvatarGeneration(); }
void ADivineBeastsCharacter::BeginPlay()
{
    Super::BeginPlay();
    Combat->OnCombatEvent.AddDynamic(this, &ADivineBeastsCharacter::HandleCombatEvent);
    CharacterIdentity->OnReadinessChanged().AddWeakLambda(this, [this](bool bReady)
    { if (!bReady) { GameplayEligibility->SetServerPlayerActive(false);
        AbilityCast->CancelConfirmedCast(AbilityCast->GetCastFacts().ActivationId); } });
    RefreshAbilityActorInfo();
}
void ADivineBeastsCharacter::PossessedBy(AController* NewController)
{ Super::PossessedBy(NewController); RefreshAbilityActorInfo(); }
void ADivineBeastsCharacter::OnRep_Controller()
{ Super::OnRep_Controller(); RefreshAbilityActorInfo(); }
void ADivineBeastsCharacter::UnPossessed()
{
    GameplayEligibility->SetServerPlayerActive(false);
    AbilityCast->CancelConfirmedCast(AbilityCast->GetCastFacts().ActivationId);
    // 只有服务器装配显式保留的同代次原角色可继续作为伤害目标；不重新初始化ASC或补充属性。
    if(DisconnectedCombatTargetGeneration<=0||DisconnectedCombatTargetGeneration!=GetCombatAvatarGeneration())AbilitySystem->ClearAbilityAvatar();
    Super::UnPossessed();
}
void ADivineBeastsCharacter::RefreshAbilityActorInfo()
{
    // 远端客户端Controller不复制给观察者，但仍需ActorInfo观察GAS属性；激活Gate继续拒绝非本地拥有者。
    if (GetController() || !HasAuthority()) { AbilitySystem->BindAbilityActorInfo(this, this); }
    else { GameplayEligibility->SetServerPlayerActive(false); if(!IsDisconnectedCombatTargetRetained())AbilitySystem->ClearAbilityAvatar(); }
}
void ADivineBeastsCharacter::HandleCombatEvent(const FGamePlatformCombatEvent& Event)
{
    if (!HasAuthority() || Event.EventType != EGamePlatformCombatEventType::Death || Event.TargetActor != this ||
        !Event.EventId.IsValid() || Event.TargetAvatarGeneration <= 0 ||
        Event.TargetAvatarGeneration != Combat->GetCombatAvatarGeneration() || !Combat->IsCombatDead()) { return; }
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
    DisconnectedCombatTargetGeneration=0;
    AuthoritativeDeath.Clear();
    GameplayEligibility->SetServerPlayerActive(false);
    CharacterIdentity->OnReadinessChanged().RemoveAll(this);
    Combat->OnCombatEvent.RemoveDynamic(this, &ADivineBeastsCharacter::HandleCombatEvent);
    AbilitySystem->ClearAbilityAvatar();
    Super::EndPlay(Reason);
}

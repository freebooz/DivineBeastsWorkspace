// SSLM V6生肖固有被动：真实服务端命中/受伤驱动已有GAS伤害增减链，未签批自动失败关闭。
#include "Components/DivineBeastsV6PassiveComponent.h"

#include "AbilitySystemComponent.h"
#include "Attributes/GamePlatformCombatAttributeSet.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformAbilitySystemComponent.h"
#include "Definitions/DivineBeastsV6PassiveDefinition.h"
#include "Effects/DivineBeastsV6PassiveGameplayEffects.h"
#include "Types/GamePlatformCombatResult.h"
#include "GameplayEffect.h"
#include "Engine/World.h"

UDivineBeastsV6PassiveComponent::UDivineBeastsV6PassiveComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
}

void UDivineBeastsV6PassiveComponent::BeginPlay()
{
    Super::BeginPlay();
    bEndingPlay = false;
    AActor* Owner = GetOwner();
    if (!IsValid(Owner) || !Owner->HasAuthority()) return;
    Identity = Owner->FindComponentByClass<UDivineBeastsCharacterComponent>();
    Combat = Owner->FindComponentByClass<UGamePlatformCombatComponent>();
    AbilitySystem = Owner->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    if (auto* IdentityPtr = Identity.Get())
    {
        ReadinessHandle = IdentityPtr->OnReadinessChanged().AddUObject(
            this, &UDivineBeastsV6PassiveComponent::HandleCharacterReadinessChanged);
        IdentityHandle = IdentityPtr->OnIdentityChanged().AddUObject(
            this, &UDivineBeastsV6PassiveComponent::HandleCharacterIdentityChanged);
    }
    if (auto* CombatPtr = Combat.Get())
    {
        CombatPtr->OnCombatEvent.AddDynamic(this, &UDivineBeastsV6PassiveComponent::HandleOwnerCombatEvent);
    }
}

void UDivineBeastsV6PassiveComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    bEndingPlay = true;
    // 先将源身份失效，再撤销自己拥有的旧效果句柄，不能触发同栈内重新启用被动。
    AuthorityDeactivate();
    if (auto* IdentityPtr = Identity.Get())
    {
        IdentityPtr->OnReadinessChanged().Remove(ReadinessHandle);
        IdentityPtr->OnIdentityChanged().Remove(IdentityHandle);
    }
    if (auto* CombatPtr = Combat.Get())
        CombatPtr->OnCombatEvent.RemoveDynamic(this, &UDivineBeastsV6PassiveComponent::HandleOwnerCombatEvent);
    ReadinessHandle.Reset();
    IdentityHandle.Reset();
    Identity.Reset();
    Combat.Reset();
    AbilitySystem.Reset();
    Super::EndPlay(Reason);
}

bool UDivineBeastsV6PassiveComponent::IsCurrentOwnerContext() const
{
    const AActor* Owner = GetOwner();
    const auto* IdentityPtr = Identity.Get();
    const auto* CombatPtr = Combat.Get();
    const auto* ASC = AbilitySystem.Get();
    const UWorld* World = GetWorld();
    return IsInGameThread() && !bEndingPlay && !bDeactivating && bPassiveActive &&
        IsValid(Owner) && Owner->HasAuthority() && !Owner->IsActorBeingDestroyed() &&
        IsValid(World) && !World->bIsTearingDown &&
        IsValid(IdentityPtr) && IdentityPtr->IsCharacterReady() &&
        IdentityPtr->GetHeroDefinitionId() == ActiveHeroId &&
        IdentityPtr->GetAvatarGeneration() == ActiveAvatarGeneration &&
        IdentityPtr->GetTrustedContextOperationId() == ActiveTrustedOperation &&
        IsValid(CombatPtr) && !CombatPtr->IsCombatDead() &&
        CombatPtr->GetCombatAvatarGeneration() == ActiveAvatarGeneration &&
        IsValid(ASC) && ASC->GetAvatarActor() == Owner &&
        IsValid(ASC->GetSet<UGamePlatformCombatAttributeSet>());
}

bool UDivineBeastsV6PassiveComponent::AuthorityConfigureFromApprovedDefinition(
    const UDivineBeastsV6PassiveDefinition& Definition, FString& OutError)
{
    OutError.Reset();
    AActor* Owner = GetOwner();
    UDivineBeastsCharacterComponent* IdentityPtr = Identity.Get();
    auto* CombatPtr = Combat.Get();
    auto* ASC = AbilitySystem.Get();
    if (!IsInGameThread() || bEndingPlay || bDeactivating || !IsValid(Owner) ||
        !Owner->HasAuthority() || !IsValid(IdentityPtr) || !IdentityPtr->IsCharacterReady() ||
        !IsValid(CombatPtr) || CombatPtr->IsCombatDead() || !IsValid(ASC) ||
        ASC->GetAvatarActor() != Owner ||
        CombatPtr->GetCombatAvatarGeneration() != IdentityPtr->GetAvatarGeneration() ||
        !Definition.ValidateForHero(IdentityPtr->GetHeroDefinitionId(), OutError))
    {
        if (OutError.IsEmpty()) OutError = TEXT("只有就绪且同代次的服务器英雄可激活已批准固有被动。");
        return false;
    }

    // 快照字段后解除原有被动；GAS同步撤销或身份通知可重入，重核后才安装新版本。
    const FGuid TrustedOperation = IdentityPtr->GetTrustedContextOperationId();
    const int32 Generation = IdentityPtr->GetAvatarGeneration();
    const FName Hero = IdentityPtr->GetHeroDefinitionId();
    const FDivineBeastsV6PassiveRules NewRules = Definition.Rules;
    const EDivineBeastsV6PassiveMode NewMode = Definition.Mode;
    AuthorityDeactivate();
    if (bEndingPlay || !IsValid(Owner) || !Owner->HasAuthority() ||
        !IsValid(IdentityPtr) || !IdentityPtr->IsCharacterReady() ||
        IdentityPtr->GetTrustedContextOperationId() != TrustedOperation ||
        IdentityPtr->GetAvatarGeneration() != Generation ||
        IdentityPtr->GetHeroDefinitionId() != Hero ||
        !IsValid(CombatPtr) || CombatPtr->GetCombatAvatarGeneration() != Generation ||
        CombatPtr->IsCombatDead() || !IsValid(ASC) || ASC->GetAvatarActor() != Owner)
    {
        OutError = TEXT("撤销旧被动时身份或化身被同步监听者接管，新被动拒绝安装。");
        return false;
    }
    Rules = NewRules;
    Mode = NewMode;
    ActiveHeroId = Hero;
    ActiveAvatarGeneration = Generation;
    ActiveTrustedOperation = TrustedOperation;
    bPassiveActive = true;
    return true;
}

bool UDivineBeastsV6PassiveComponent::TryRememberEvent(FGuid EventId)
{
    if (!EventId.IsValid() || SeenCombatEvents.Contains(EventId))
        return false;
    SeenCombatEvents.Add(EventId);
    SeenCombatEventOrder.Add(EventId);
    while (SeenCombatEventOrder.Num() > 128)
    {
        const FGuid Oldest = SeenCombatEventOrder[0];
        SeenCombatEventOrder.RemoveAt(0);
        SeenCombatEvents.Remove(Oldest);
    }
    return true;
}

bool UDivineBeastsV6PassiveComponent::AuthorityOnConfirmedDamageHit(
    AActor* Target, int32 TargetAvatarGeneration,
    const FGamePlatformCombatResult& Result)
{
    if (!IsCurrentOwnerContext() || Mode != EDivineBeastsV6PassiveMode::TigerFracture ||
        !Result.IsSuccess() || !Result.EventId.IsValid() ||
        !FMath::IsFinite(Result.AppliedToHealth) || !FMath::IsFinite(Result.AppliedToShield) ||
        Result.AppliedToHealth + Result.AppliedToShield < Rules.MinimumConfirmedDamage ||
        !IsValid(Target) || Target == GetOwner() ||
        Target->GetWorld() != GetWorld() || TargetAvatarGeneration <= 0)
        return false;
    const auto* Hero = Cast<ADivineBeastsCharacter>(GetOwner());
    auto* TargetCombat = Target->FindComponentByClass<UGamePlatformCombatComponent>();
    auto* TargetASC = Target->FindComponentByClass<UGamePlatformAbilitySystemComponent>();
    if (!IsValid(Hero) || Hero->AuthorityIsFriendlyCombatTarget(Target) ||
        !IsValid(TargetCombat) || TargetCombat->IsCombatDead() ||
        TargetCombat->GetCombatAvatarGeneration() != TargetAvatarGeneration ||
        !IsValid(TargetASC) || TargetASC->GetAvatarActor() != Target ||
        !IsValid(TargetASC->GetSet<UGamePlatformCombatAttributeSet>()) ||
        !TryRememberEvent(Result.EventId))
        return false;

    // 换猎物必须取消自己旧裂甲，防止不相关目标并行叠满和旧目标失控残留。
    if (FractureTarget.Get() != Target || FractureTargetAvatarGeneration != TargetAvatarGeneration)
    {
        if (auto* OldASC = FractureTargetASC.Get())
            if (IsValid(FractureTarget.Get()) && FractureEffect.IsValid() &&
                OldASC->GetAvatarActor() == FractureTarget.Get())
            {
                const auto* OldCombat =
                    FractureTarget->FindComponentByClass<UGamePlatformCombatComponent>();
                if (IsValid(OldCombat) &&
                    OldCombat->GetCombatAvatarGeneration() == FractureTargetAvatarGeneration)
                    OldASC->RemoveActiveGameplayEffect(FractureEffect);
            }
        FractureTarget.Reset();
        FractureTargetASC.Reset();
        FractureEffect.Invalidate();
        FractureTargetAvatarGeneration = 0;
    }
    // 移除上一猎物的GE会同步调用外部监听者；后继绑定、角色退出和目标换代均需重新核实。
    if (!IsCurrentOwnerContext() || !IsValid(Target) || !IsValid(TargetASC) ||
        !IsValid(TargetCombat) || TargetCombat->IsCombatDead() ||
        TargetASC->GetAvatarActor() != Target ||
        TargetCombat->GetCombatAvatarGeneration() != TargetAvatarGeneration)
        return false;
    if (FractureEffect.IsValid() &&
        TargetASC->GetCurrentStackCount(FractureEffect) >= Rules.MaxStacks)
        return false;

    auto* SourceASC = AbilitySystem.Get();
    if (!IsValid(SourceASC) || SourceASC->GetAvatarActor() != GetOwner()) return false;
    const FGuid Operation = ActiveTrustedOperation;
    FGameplayEffectSpecHandle GE = SourceASC->MakeOutgoingSpec(
        UDivineBeastsV6TigerFractureGameplayEffect::StaticClass(), 1.0f,
        SourceASC->MakeEffectContext());
    if (!GE.IsValid() || !GE.Data.IsValid()) return false;
    GE.Data->SetDuration(Rules.DurationSeconds, true);
    GE.Data->SetSetByCallerMagnitude(
        UDivineBeastsV6TigerFractureGameplayEffect::MagnitudeDataName(),
        -Rules.PerStackMagnitude);
    const FActiveGameplayEffectHandle Applied =
        SourceASC->ApplyGameplayEffectSpecToTarget(*GE.Data.Get(), TargetASC);
    if (!Applied.WasSuccessfullyApplied()) return false;
    if (!IsCurrentOwnerContext() || ActiveTrustedOperation != Operation ||
        !IsValid(Target) || TargetCombat->GetCombatAvatarGeneration() != TargetAvatarGeneration ||
        TargetASC->GetAvatarActor() != Target)
    {
        if (IsValid(TargetASC) && TargetASC->GetAvatarActor() == Target)
            TargetASC->RemoveActiveGameplayEffect(Applied);
        return false;
    }
    FractureTarget = Target;
    FractureTargetASC = TargetASC;
    FractureTargetAvatarGeneration = TargetAvatarGeneration;
    FractureEffect = Applied;
    return true;
}

void UDivineBeastsV6PassiveComponent::HandleOwnerCombatEvent(const FGamePlatformCombatEvent& Event)
{
    if (!IsCurrentOwnerContext() || Mode != EDivineBeastsV6PassiveMode::BoarBattleWill ||
        Event.EventType != EGamePlatformCombatEventType::Damage ||
        Event.TargetActor != GetOwner() || Event.SourceActor == GetOwner() ||
        Event.TargetAvatarGeneration != ActiveAvatarGeneration ||
        !FMath::IsFinite(Event.AppliedToHealth) || !FMath::IsFinite(Event.AppliedToShield) ||
        Event.AppliedToHealth + Event.AppliedToShield < Rules.MinimumConfirmedDamage ||
        !TryRememberEvent(Event.EventId))
        return;

    UGamePlatformAbilitySystemComponent* ASC = AbilitySystem.Get();
    if (BattleWillEffect.IsValid() && ASC->GetCurrentStackCount(BattleWillEffect) >= Rules.MaxStacks)
        return;
    const FGuid Operation = ActiveTrustedOperation;
    FGameplayEffectSpecHandle GE = ASC->MakeOutgoingSpec(
        UDivineBeastsV6BoarBattleWillGameplayEffect::StaticClass(), 1.0f,
        ASC->MakeEffectContext());
    if (!GE.IsValid() || !GE.Data.IsValid()) return;
    GE.Data->SetDuration(Rules.DurationSeconds, true);
    GE.Data->SetSetByCallerMagnitude(
        UDivineBeastsV6BoarBattleWillGameplayEffect::MagnitudeDataName(),
        Rules.PerStackMagnitude);
    const FActiveGameplayEffectHandle Applied = ASC->ApplyGameplayEffectSpecToSelf(*GE.Data.Get());
    if (!Applied.WasSuccessfullyApplied()) return;
    if (!IsCurrentOwnerContext() || ActiveTrustedOperation != Operation)
    {
        if (IsValid(ASC) && ASC->GetAvatarActor() == GetOwner())
            ASC->RemoveActiveGameplayEffect(Applied);
        return;
    }
    BattleWillEffect = Applied;
}

void UDivineBeastsV6PassiveComponent::HandleCharacterReadinessChanged(bool bReady)
{
    if (!bReady) AuthorityDeactivate();
}
void UDivineBeastsV6PassiveComponent::HandleCharacterIdentityChanged()
{
    AuthorityDeactivate();
}

void UDivineBeastsV6PassiveComponent::AuthorityDeactivate()
{
    if (bDeactivating) return;
    bDeactivating = true;
    bPassiveActive = false;
    // 先失效被动资格，再撤销由本组件产生的旧GE；不得移除其他技能/来源的效果。
    auto* ASC = AbilitySystem.Get();
    if (BattleWillEffect.IsValid() && IsValid(ASC) &&
        ASC->GetAvatarActor() == GetOwner())
        ASC->RemoveActiveGameplayEffect(BattleWillEffect);
    if (FractureEffect.IsValid() && IsValid(FractureTargetASC.Get()) &&
        IsValid(FractureTarget.Get()) &&
        FractureTargetASC->GetAvatarActor() == FractureTarget.Get())
    {
        const auto* TargetCombat =
            FractureTarget->FindComponentByClass<UGamePlatformCombatComponent>();
        if (IsValid(TargetCombat) &&
            TargetCombat->GetCombatAvatarGeneration() == FractureTargetAvatarGeneration)
            FractureTargetASC->RemoveActiveGameplayEffect(FractureEffect);
    }
    BattleWillEffect.Invalidate();
    FractureEffect.Invalidate();
    FractureTarget.Reset();
    FractureTargetASC.Reset();
    FractureTargetAvatarGeneration = 0;
    SeenCombatEvents.Reset();
    SeenCombatEventOrder.Reset();
    ActiveHeroId = NAME_None;
    ActiveAvatarGeneration = 0;
    ActiveTrustedOperation.Invalidate();
    Mode = EDivineBeastsV6PassiveMode::Unsupported;
    Rules = {};
    bDeactivating = false;
}

// SSLM十二生肖占位技能：复用项目Definition和平台唯一权威投射物，不启用开发技能后门。
#include "Abilities/DivineBeastsPlaceholderGameplayAbility.h"

#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/GamePlatformCombatant.h"

UDivineBeastsPlaceholderGameplayAbility::UDivineBeastsPlaceholderGameplayAbility()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UGameplayEffect* UDivineBeastsPlaceholderGameplayAbility::GetCostGameplayEffect() const
{
    // 不退款、不补资源、不绕过Commit；只排除用户已明确取消的法术气势成本，原有冷却仍走GAS。
    if (PlaceholderMode==EDivineBeastsPlaceholderAbilityMode::Fireball ||
        PlaceholderMode==EDivineBeastsPlaceholderAbilityMode::Frostbolt ||
        PlaceholderMode==EDivineBeastsPlaceholderAbilityMode::ArcaneMissiles ||
        PlaceholderMode==EDivineBeastsPlaceholderAbilityMode::Healing) return nullptr;
    return Super::GetCostGameplayEffect();
}

bool UDivineBeastsPlaceholderGameplayAbility::CheckCost(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    FGameplayTagContainer* OptionalRelevantTags) const
{
    // 占位被动既不扣除GAS成本也不伪装为可主动释放；正式被动能力另行替换。
    return PlaceholderMode != EDivineBeastsPlaceholderAbilityMode::Passive &&
        Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags);
}

const FGameplayTagContainer* UDivineBeastsPlaceholderGameplayAbility::GetCooldownTags() const
{
    return &PlaceholderCooldownTags;
}

void UDivineBeastsPlaceholderGameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const FGuid Token = FGuid::NewGuid();
    ActivationToken = Token;
    // 基类清理上一次Commit状态；严禁调用开发技能基类，避免修改Shipping准入策略。
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    if (ActivationToken != Token || !IsActive()) return;

    AActor* Avatar = GetAvatarActorFromActorInfo();
    auto* Combat = IsValid(Avatar) ? Avatar->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    UWorld* World = IsValid(Avatar) ? Avatar->GetWorld() : nullptr;
    const int32 AvatarGeneration = Combat ? Combat->GetCombatAvatarGeneration() : 0;
    FDivineBeastsAbilityBalanceRow Balance;
    FString Error;
    const bool bHealing = PlaceholderMode == EDivineBeastsPlaceholderAbilityMode::Healing;

    if (!Avatar || !Avatar->HasAuthority() || !Combat || Combat->IsCombatDead() ||
        !World || World->bIsTearingDown || AvatarGeneration <= 0 ||
        PlaceholderMode == EDivineBeastsPlaceholderAbilityMode::Passive ||
        !TryReadConfiguredBalance(Balance, Error) ||
        (!bHealing && Balance.BaseDamage <= 0.0f) || Balance.CastRangeCm <= 0.0f ||
        (bHealing && (!FMath::IsNearlyEqual(Balance.CooldownSeconds, 10.f) ||
            Balance.MomentumCost != 0.f || !GetCooldownGameplayEffect() || PlaceholderCooldownTags.IsEmpty())))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const bool bProjectile = !bHealing &&
        PlaceholderMode != EDivineBeastsPlaceholderAbilityMode::Primary;
    if (bProjectile &&
        (!ProjectileMotion.IsValid() ||
         !FMath::IsNearlyEqual(ProjectileMotion.RangeCm, Balance.CastRangeCm) ||
         ProjectileCount < 1 || ProjectileCount > 3 ||
         ((ProjectileCount == 1 && ProjectileIntervalSeconds != 0.0f) ||
          (ProjectileCount > 1 &&
           (ProjectileIntervalSeconds < 0.05f || ProjectileIntervalSeconds > 1.0f)))))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // 先冻结治疗目标并检查资格，再提交冷却。失效的已选友方不能被过滤为nullptr后误自疗。
    auto* HealingSource = bHealing ? Cast<ADivineBeastsCharacter>(Avatar) : nullptr;
    TWeakObjectPtr<AActor> HealingTarget = Avatar;
    int32 HealingTargetGeneration = AvatarGeneration;
    if (bHealing)
    {
        TWeakObjectPtr<AActor> IntentTarget;
        int32 IntentSourceGeneration = 0, IntentTargetGeneration = 0;
        const bool bHasIntent = Combat->GetSelectedProjectileTargetIntent(IntentTarget, IntentSourceGeneration, IntentTargetGeneration);
        if (!HealingSource || (bHasIntent && (!IntentTarget.IsValid() || IntentSourceGeneration != AvatarGeneration)))
        { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
        if (bHasIntent && HealingSource->AuthorityIsFriendlyCombatTarget(IntentTarget.Get()))
        { HealingTarget = IntentTarget; HealingTargetGeneration = IntentTargetGeneration; }
    }
    const auto IsHealingTargetCurrent = [&]()
    {
        AActor* Target = HealingTarget.Get();
        const auto* TargetCombat = IsValid(Target) ? Target->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
        const auto* TargetPolicy = IsValid(Target) ? Cast<IGamePlatformCombatant>(Target) : nullptr;
        return IsValid(HealingSource) && IsValid(Target) && !Target->IsActorBeingDestroyed() &&
            Target->GetWorld() == World && TargetCombat && !TargetCombat->IsCombatDead() &&
            (!TargetPolicy || TargetPolicy->CanReceiveCombat()) &&
            TargetCombat->GetCombatAvatarGeneration() == HealingTargetGeneration &&
            HealingSource->AuthorityIsFriendlyCombatTarget(Target) &&
            !Target->GetActorLocation().ContainsNaN() && !Avatar->GetActorLocation().ContainsNaN() &&
            FVector::DistSquared(Avatar->GetActorLocation(), Target->GetActorLocation()) <= FMath::Square(Balance.CastRangeCm) &&
            FMath::IsFinite(TargetCombat->GetCombatMaxHealth()) && TargetCombat->GetCombatMaxHealth() > 0.f;
    };
    if (bHealing && !IsHealingTargetCurrent())
    { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }

    const bool bCommitted = CommitAbility(Handle, ActorInfo, ActivationInfo);
    // 成本/冷却回调可能同步结束或替换激活；只允许本次已Commit的化身执行权威伤害。
    if (ActivationToken != Token || !IsActive()) return;
    if (!bCommitted || !IsValid(Avatar) || GetAvatarActorFromActorInfo() != Avatar ||
        Avatar->GetWorld() != World || World->bIsTearingDown || !IsValid(Combat) ||
        Combat->IsCombatDead() || Combat->GetCombatAvatarGeneration() != AvatarGeneration)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    if (bHealing)
    {
        // Commit通知可能同步改变友方关系、目标化身或世界；不能把旧资格用于新目标，也不伪造退款。
        if (!IsHealingTargetCurrent())
        { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
        AActor* Target = HealingTarget.Get();
        const auto* TargetCombat = Target->FindComponentByClass<UGamePlatformCombatComponent>();
        FGamePlatformCombatSpec HealingSpec;
        HealingSpec.EventId = Token; HealingSpec.Source = Avatar; HealingSpec.Target = Target;
        HealingSpec.SourceAbilityId = Balance.AbilityId;
        HealingSpec.SourceAvatarGeneration = AvatarGeneration;
        HealingSpec.TargetAvatarGeneration = HealingTargetGeneration;
        HealingSpec.Magnitude = TargetCombat->GetCombatMaxHealth() * .2f;
        // 唯一平台GAS结算负责上限、死亡拒绝、复制和生命事件；治疗不触发普攻命中奖励。
        const FGamePlatformCombatResult Result = Combat->ApplyHealing(HealingSpec);
        if (ActivationToken != Token || !IsActive()) return;
        if (Result.IsSuccess() && IsValid(Avatar) && GetAvatarActorFromActorInfo() == Avatar &&
            IsValid(Combat) && Combat->GetCombatAvatarGeneration() == AvatarGeneration)
            if (auto* Facts = Avatar->FindComponentByClass<UGamePlatformAbilityCastComponent>())
                Facts->PublishConfirmedCast(Token, Balance.AbilityId, TEXT("DBA.Development.Healing"), AvatarGeneration, .7f);
        if (ActivationToken == Token && IsActive())
            EndAbility(Handle, ActorInfo, ActivationInfo, true, !Result.IsSuccess());
        return;
    }

    if (!bProjectile)
    {
        FGamePlatformCombatResult Result;
        const bool bHit = AuthorityTraceForwardAndApplyDamage(Result, Error);
        // 仅成功命中、已扣血或耗盾的占位普攻触发气势奖励，平台Combat负责伤害结算。
        if (bHit && ActivationToken == Token && IsActive() && IsValid(Avatar) &&
            GetAvatarActorFromActorInfo() == Avatar && IsValid(Combat) &&
            Combat->GetCombatAvatarGeneration() == AvatarGeneration)
        {
            if (UDivineBeastsCharacterComponent* Identity =
                    Avatar->FindComponentByClass<UDivineBeastsCharacterComponent>())
            {
                Identity->AuthorityAwardMomentumForConfirmedBasicHit(Result, AvatarGeneration);
            }
        }
        if (ActivationToken == Token && IsActive())
            EndAbility(Handle, ActorInfo, ActivationInfo, true, !bHit);
        return;
    }

    CastSpec = {};
    CastSpec.Source = Avatar;
    CastSpec.Magnitude = Balance.BaseDamage;
    CastSpec.DamageType = Balance.DamageType;
    CastSpec.SourceAbilityId = Balance.AbilityId;
    CastSpec.SourceAvatarGeneration = AvatarGeneration;
    // 兼容已批准的原生开发样板，同样只追踪服务器冻结的选中目标。
    FrozenProjectileTarget=Combat->GetSelectedProjectileTarget(Balance.CastRangeCm,CastSpec.TargetAvatarGeneration);
    bHadFrozenTarget=FrozenProjectileTarget.IsValid();
    CastSpec.Target=nullptr; // Timer之间不持有目标裸/强引用；每枚在释放时从弱身份重新验证。
    RemainingShots = ProjectileCount;
    ReleasedShots = 0;
    // 沿用既有平台表现语义；客户端缺少可选Niagara时服务器结果不变。
    switch (PlaceholderMode)
    {
    case EDivineBeastsPlaceholderAbilityMode::Fireball:
        CastStyle = TEXT("DBA.Development.Fireball"); break;
    case EDivineBeastsPlaceholderAbilityMode::Frostbolt:
        CastStyle = TEXT("DBA.Development.Frostbolt"); break;
    case EDivineBeastsPlaceholderAbilityMode::ArcaneMissiles:
        CastStyle = TEXT("DBA.Development.ArcaneMissiles"); break;
    default:
        CastStyle = NAME_None; break;
    }
    if (auto* CastFacts = Avatar->FindComponentByClass<UGamePlatformAbilityCastComponent>())
    {
        CastFacts->PublishConfirmedCast(Token, Balance.AbilityId, CastStyle, AvatarGeneration,
            PlaceholderMode == EDivineBeastsPlaceholderAbilityMode::Frostbolt ?
                UDivineBeastsDevelopmentMagicDefinition::FrostboltCastDurationSeconds : (RemainingShots > 1 ? 1.0f : 0.7f));
    }
    if (ActivationToken != Token || !IsActive()) return;
    if (PlaceholderMode == EDivineBeastsPlaceholderAbilityMode::Frostbolt)
    {
        // 冰箭完成用户指定的2秒权威起手再释放；复制施法条与释放共用时长，取消/换代仍统一拒绝。
        World->GetTimerManager().SetTimer(BurstTimer, FTimerDelegate::CreateWeakLambda(this, [this, Token]()
            { if (ActivationToken == Token) ReleaseNextProjectile(); }),
            UDivineBeastsDevelopmentMagicDefinition::FrostboltCastDurationSeconds, false);
        return;
    }
    ReleaseNextProjectile();
}

void UDivineBeastsPlaceholderGameplayAbility::ReleaseNextProjectile()
{
    const FGuid Token = ActivationToken;
    AActor* Source = CastSpec.Source.Get();
    UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
    auto* Combat = IsValid(Source) ? Source->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    if (!IsActive() || !IsValid(Source) || !Source->HasAuthority() || !World ||
        World->bIsTearingDown || !Combat || Combat->IsCombatDead() ||
        Combat->GetCombatAvatarGeneration() != CastSpec.SourceAvatarGeneration ||
        RemainingShots <= 0)
    {
        if (IsActive()) EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }

    AActor* Target=FrozenProjectileTarget.Get();
    const auto* TargetCombat=IsValid(Target) ? Target->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    if (bHadFrozenTarget && (!IsValid(Target) || Target->IsActorBeingDestroyed() || Target->GetWorld()!=World ||
        !TargetCombat || TargetCombat->IsCombatDead() || TargetCombat->GetCombatAvatarGeneration()!=CastSpec.TargetAvatarGeneration))
    { EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,true); return; }
    const FVector AimOrigin=Source->GetActorLocation()+FVector(0,0,50);
    const FVector Direction = bHadFrozenTarget ?
        (Target->GetActorLocation()-AimOrigin).GetSafeNormal() : Source->GetActorForwardVector().GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }
    const FVector Start = Source->GetActorLocation() + FVector(0, 0, 50) + Direction * 50;
    FActorSpawnParameters Spawn;
    Spawn.Owner = Source;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    FGamePlatformCombatSpec Shot = CastSpec;
    Shot.EventId = FGuid::NewGuid();
    FGamePlatformCombatProjectileMotion ShotMotion = ProjectileMotion;
    if (ProjectileMotion.HelixRadiusCm > 0)
        ShotMotion.InitialPhaseTurns = ReleasedShots / 3.0f;
    auto* Projectile = World->SpawnActor<AGamePlatformCombatProjectile>(
        Start, Direction.Rotation(), Spawn);

    if (Token != ActivationToken || !IsActive() || !IsValid(Source) ||
        GetAvatarActorFromActorInfo() != Source || !IsValid(Combat) ||
        Combat->IsCombatDead() || Combat->GetCombatAvatarGeneration() != Shot.SourceAvatarGeneration)
    {
        if (Projectile) Projectile->Destroy();
        if (Token == ActivationToken && IsActive())
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }
    // Spawn通知可能销毁目标；冻结身份失效时取消，不能把被GC置空的目标当作最初未选择。
    Shot.Target=FrozenProjectileTarget.Get();
    if (bHadFrozenTarget && !IsValid(Shot.Target.Get()))
    {
        if (Projectile) Projectile->Destroy();
        if (Token==ActivationToken && IsActive())
            EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,true);
        return;
    }
    if (!Projectile || !Projectile->AuthorityInitialize(Shot, ShotMotion, Direction, CastStyle))
    {
        if (Projectile) Projectile->Destroy();
        if (Token == ActivationToken && IsActive())
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }
    if (Token != ActivationToken || !IsActive()) return;
    ++ReleasedShots;
    --RemainingShots;
    if (RemainingShots == 0)
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
        return;
    }
    World->GetTimerManager().SetTimer(BurstTimer, FTimerDelegate::CreateWeakLambda(
        this, [this, Token]()
        {
            if (ActivationToken == Token) ReleaseNextProjectile();
        }), ProjectileIntervalSeconds, false);
}

void UDivineBeastsPlaceholderGameplayAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    const FGuid Ended = ActivationToken;
    ActivationToken.Invalidate();
    if (AActor* Source = CastSpec.Source.Get(); IsValid(Source) && Source->GetWorld())
        Source->GetWorld()->GetTimerManager().ClearTimer(BurstTimer);
    BurstTimer.Invalidate(); FrozenProjectileTarget.Reset(); bHadFrozenTarget=false;
    RemainingShots = 0;
    CastSpec = {};
    CastStyle = NAME_None;
    if (bWasCancelled && ActorInfo && ActorInfo->AvatarActor.IsValid())
        if (auto* Facts = ActorInfo->AvatarActor->FindComponentByClass<UGamePlatformAbilityCastComponent>())
            Facts->CancelConfirmedCast(Ended);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// SSLM项目服务器唯一施法状态机：数据归适配，权威运动/碰撞/结算仍归GamePlatformCombat。
#include "Abilities/DivineBeastsCombatGameplayAbility.h"

#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/GamePlatformCombatant.h"

UDivineBeastsCombatGameplayAbility::UDivineBeastsCombatGameplayAbility()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

bool UDivineBeastsCombatGameplayAbility::TryBuildCombatExecution(
    const FDivineBeastsAbilityBalanceRow& Balance,
    FDivineBeastsCombatExecutionParameters& OutExecution, FString& OutError) const
{
    // 抽象执行器不能靠默认值成为可释放技能；具体适配必须提供已批准规则。
    OutError = TEXT("缺少项目战斗执行参数适配。");
    return false;
}

void UDivineBeastsCombatGameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const FGuid Token = FGuid::NewGuid();
    ActivationToken = Token;
    // 唯一配置基类清理Commit状态；不执行开发射线入口，也不改变各适配自己的资格。
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    if (ActivationToken != Token || !IsActive()) return;

    AActor* Avatar = GetAvatarActorFromActorInfo();
    auto* Combat = IsValid(Avatar) ? Avatar->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    UWorld* World = IsValid(Avatar) ? Avatar->GetWorld() : nullptr;
    const int32 AvatarGeneration = Combat ? Combat->GetCombatAvatarGeneration() : 0;
    FDivineBeastsAbilityBalanceRow Balance;
    FString Error;
    FDivineBeastsCombatExecutionParameters Execution;
    const bool bHasExecution = TryReadConfiguredBalance(Balance, Error) &&
        TryBuildCombatExecution(Balance, Execution, Error);
    const bool bHealing = Execution.Mode == EDivineBeastsCombatExecution::Healing;

    if (!Avatar || !Avatar->HasAuthority() || !Combat || Combat->IsCombatDead() ||
        !World || World->bIsTearingDown || AvatarGeneration <= 0 ||
        !bHasExecution || (!bHealing && Balance.BaseDamage <= 0.0f) || Balance.CastRangeCm <= 0.0f)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    const bool bProjectile = Execution.Mode == EDivineBeastsCombatExecution::Projectile;
    if (bProjectile &&
        (!Execution.Motion.IsValid() ||
         !UDivineBeastsDevelopmentMagicDefinition::IsProjectileSpeedSupported(Execution.Motion.SpeedCmPerSecond) ||
         !FMath::IsNearlyEqual(Execution.Motion.RangeCm, Balance.CastRangeCm) ||
         Execution.ProjectileCount < 1 || Execution.ProjectileCount > 3 ||
         !FMath::IsFinite(Execution.ProjectileIntervalSeconds) ||
         !FMath::IsFinite(Execution.CastDurationSeconds) || Execution.CastDurationSeconds < 0.f ||
         ((Execution.ProjectileCount == 1 && Execution.ProjectileIntervalSeconds != 0.0f) ||
          (Execution.ProjectileCount > 1 &&
           (Execution.ProjectileIntervalSeconds < 0.05f || Execution.ProjectileIntervalSeconds > 1.0f)))))
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

    // 在Commit前冻结选择意图。已选目标超距/失效必须拒绝，不能过滤成nullptr后改射直线并消耗冷却。
    TWeakObjectPtr<AActor> SelectedCastTarget;
    int32 SelectedSourceGeneration = 0, SelectedTargetGeneration = 0;
    const bool bSelectedCastTarget = bProjectile && Combat->GetSelectedProjectileTargetIntent(
        SelectedCastTarget, SelectedSourceGeneration, SelectedTargetGeneration);
    int32 ValidatedTargetGeneration = 0;
    if (bSelectedCastTarget && (!SelectedCastTarget.IsValid() || SelectedSourceGeneration != AvatarGeneration ||
        Combat->GetSelectedProjectileTarget(Balance.CastRangeCm, ValidatedTargetGeneration) != SelectedCastTarget.Get() ||
        ValidatedTargetGeneration != SelectedTargetGeneration))
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
    // Commit回调即使改选，也不能改写本次已经校验的原目标。
    FrozenProjectileTarget=SelectedCastTarget;
    CastSpec.TargetAvatarGeneration=SelectedTargetGeneration;
    bHadFrozenTarget=bSelectedCastTarget;
    CastSpec.Target=nullptr; // Timer之间不持有目标裸/强引用；每枚在释放时从弱身份重新验证。
    ActiveExecution = Execution;
    RemainingShots = Execution.ProjectileCount;
    ReleasedShots = 0;
    CastStyle = Execution.PresentationStyle;
    // 权威起手和施法事实使用同一快照；奥术首发保持立即，连发使用独立间隔。
    const float CastDurationSeconds = Execution.CastDurationSeconds;
    if (auto* CastFacts = Avatar->FindComponentByClass<UGamePlatformAbilityCastComponent>())
    {
        CastFacts->PublishConfirmedCast(Token, Balance.AbilityId, CastStyle, AvatarGeneration,
            CastDurationSeconds > 0.0f ? CastDurationSeconds : (RemainingShots > 1 ? 1.0f : 0.7f));
    }
    if (ActivationToken != Token || !IsActive()) return;
    if (CastDurationSeconds > 0.0f)
    {
        // 复用自有BurstTimer等待首次释放；后续连发仍由ReleaseNextProjectile按原间隔接续。
        World->GetTimerManager().SetTimer(BurstTimer, FTimerDelegate::CreateWeakLambda(this, [this, Token]()
            { if (ActivationToken == Token) ReleaseNextProjectile(); }),
            CastDurationSeconds, false);
        return;
    }
    ReleaseNextProjectile();
}

void UDivineBeastsCombatGameplayAbility::ReleaseNextProjectile()
{
    const FGuid Token = ActivationToken;
    AActor* Source = CastSpec.Source.Get();
    UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
    auto* Combat = IsValid(Source) ? Source->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    if (!IsActive() || !IsValid(Source) || GetAvatarActorFromActorInfo() != Source ||
        !Source->HasAuthority() || !World ||
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
    // 初始切线遵循服务器施法朝向；平台弹体自行构建弧线并追踪冻结目标，不在此把方向瞬间改向目标。
    const FVector Direction = Source->GetActorForwardVector().GetSafeNormal();
    if (Direction.IsNearlyZero())
    {
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }
    const FVector Start = AimOrigin + Direction * 50;
    FActorSpawnParameters Spawn;
    Spawn.Owner = Source;
    Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    FGamePlatformCombatSpec Shot = CastSpec;
    Shot.EventId = FGuid::NewGuid();
    FGamePlatformCombatProjectileMotion ShotMotion = ActiveExecution.Motion;
    if (ActiveExecution.Motion.HelixRadiusCm > 0)
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
        }), ActiveExecution.ProjectileIntervalSeconds, false);
}

void UDivineBeastsCombatGameplayAbility::EndAbility(
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
    ActiveExecution = {};
    if (Ended.IsValid() && bWasCancelled && ActorInfo && ActorInfo->AvatarActor.IsValid())
        if (auto* Facts = ActorInfo->AvatarActor->FindComponentByClass<UGamePlatformAbilityCastComponent>())
            Facts->CancelConfirmedCast(Ended);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

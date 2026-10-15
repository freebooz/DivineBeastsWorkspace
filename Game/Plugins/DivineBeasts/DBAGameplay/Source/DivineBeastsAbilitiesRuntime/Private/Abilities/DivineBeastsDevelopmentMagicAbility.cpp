// 项目开发施法：数据与输入身份归项目，运动/碰撞/护盾/生命结算归平台，无Niagara客户端依赖。
#include "Abilities/DivineBeastsDevelopmentMagicAbility.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "Development/DivineBeastsDevelopmentAbilityPolicy.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"

UGameplayEffect* UDivineBeastsDevelopmentMagicAbility::GetCostGameplayEffect() const
{
    // 使用原生GAS无成本合同；不是跳过Commit，也不在施法后退款或补气势。
    // 即使蓝图遗留费用引用，服务器与客户端的资源资格检查均不会扣除或要求气势。
    return nullptr;
}

void UDivineBeastsDevelopmentMagicAbility::ActivateAbility(FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const FGuid ActivationToken=FGuid::NewGuid(); CastActivationToken=ActivationToken;
    // 复用配置技能的Commit代次初始化，不能调用父开发样板的“立刻射线/结束”动作。
    UDivineBeastsConfiguredGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    if (CastActivationToken!=ActivationToken || !IsActive()) return;
    AActor* Avatar = GetAvatarActorFromActorInfo();
    auto* Combat = IsValid(Avatar) ? Avatar->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    const auto* Definition = Cast<UDivineBeastsDevelopmentMagicDefinition>(UAssetManager::Get().GetPrimaryAssetObject(AbilityDefinitionId));
    FDivineBeastsAbilityBalanceRow Balance; FString Error;
    UWorld* CastWorld=IsValid(Avatar) ? Avatar->GetWorld() : nullptr;
    const int32 SourceGeneration=Combat ? Combat->GetCombatAvatarGeneration() : 0;
    if (!DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess() || !Avatar || !Avatar->HasAuthority() ||
        !Combat || Combat->IsCombatDead() || !Definition || !Definition->ValidateDefinition().IsSuccess() ||
        !TryReadConfiguredBalance(Balance, Error) || Balance.BaseDamage <= 0 ||
        !CastWorld || CastWorld->bIsTearingDown || SourceGeneration<=0 ||
        !FMath::IsNearlyEqual(Balance.CastRangeCm, Definition->ProjectileMotion.RangeCm))
    { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
    const bool bCommitted=CommitAbility(Handle, ActorInfo, ActivationInfo);
    // 原生GAS同步成本/冷却回调可能取消施法；不沿用Commit前资格生成投射物。
    if (CastActivationToken!=ActivationToken || !IsActive()) return;
    if (!bCommitted || !IsValid(Avatar) || GetAvatarActorFromActorInfo()!=Avatar || Avatar->GetWorld()!=CastWorld ||
        CastWorld->bIsTearingDown || !IsValid(Combat) || Combat->IsCombatDead() || Combat->GetCombatAvatarGeneration()!=SourceGeneration)
    { EndAbility(Handle,ActorInfo,ActivationInfo,true,true); return; }
    CastSpec = {}; CastSpec.Source = Avatar; CastSpec.Magnitude = Balance.BaseDamage;
    CastSpec.DamageType = Balance.DamageType; CastSpec.SourceAbilityId = Balance.AbilityId;
    CastSpec.SourceAvatarGeneration = SourceGeneration;
    // 只冻结服务器已验证的当前选择；未选/失效/超射程保持直线，不扫描周围敌人。
    FrozenProjectileTarget=Combat->GetSelectedProjectileTarget(Balance.CastRangeCm,CastSpec.TargetAvatarGeneration);
    bHadFrozenTarget=FrozenProjectileTarget.IsValid();
    CastSpec.Target=nullptr; // Timer之间不持有目标裸/强引用；每枚在释放时从弱身份重新验证。
    Motion = Definition->ProjectileMotion; StyleId = Definition->GetPresentationStyleId();
    RemainingProjectiles = Definition->ProjectileCount; ReleasedProjectiles = 0;
    IntervalSeconds = Definition->ProjectileIntervalSeconds;
    // 表现窗口与弹体时序解耦：原有瞬发/连发不变，可选动画失败不能影响权威伤害。
    if (auto* CastFacts=Avatar->FindComponentByClass<UGamePlatformAbilityCastComponent>())
        CastFacts->PublishConfirmedCast(ActivationToken,Balance.AbilityId,StyleId,SourceGeneration,
            RemainingProjectiles>1 ? 1.0f : 0.7f);
    // 发布委托可能同步取消并重新激活；旧发布栈不得释放后继激活的弹体。
    if (CastActivationToken!=ActivationToken || !IsActive()) return;
    ReleaseNextProjectile();
}

void UDivineBeastsDevelopmentMagicAbility::ReleaseNextProjectile()
{
    const FGuid ActivationToken=CastActivationToken;
    AActor* Source = CastSpec.Source.Get();
    auto* Combat = IsValid(Source) ? Source->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    UWorld* World = Source ? Source->GetWorld() : nullptr;
    if (!IsActive() || !IsValid(Source) || !Source->HasAuthority() || !World || World->bIsTearingDown ||
        !Combat || Combat->IsCombatDead() || Combat->GetCombatAvatarGeneration() != CastSpec.SourceAvatarGeneration || RemainingProjectiles <= 0)
    { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
    AActor* Target=FrozenProjectileTarget.Get();
    const auto* TargetCombat=IsValid(Target) ? Target->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    if (bHadFrozenTarget && (!IsValid(Target) || Target->IsActorBeingDestroyed() || Target->GetWorld()!=World ||
        !TargetCombat || TargetCombat->IsCombatDead() || TargetCombat->GetCombatAvatarGeneration()!=CastSpec.TargetAvatarGeneration))
    { EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,true); return; }
    const FVector AimOrigin=Source->GetActorLocation()+FVector(0,0,50);
    const FVector Direction = bHadFrozenTarget ?
        (Target->GetActorLocation()-AimOrigin).GetSafeNormal() : Source->GetActorForwardVector().GetSafeNormal();
    const FVector Start = Source->GetActorLocation() + FVector(0, 0, 50) + Direction * 50;
    FActorSpawnParameters Spawn; Spawn.Owner = Source; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    FGamePlatformCombatSpec Shot = CastSpec; Shot.EventId = FGuid::NewGuid();
    FGamePlatformCombatProjectileMotion ShotMotion = Motion;
    if (Motion.HelixRadiusCm > 0) ShotMotion.InitialPhaseTurns = ReleasedProjectiles / 3.0f;
    auto* Projectile = World->SpawnActor<AGamePlatformCombatProjectile>(Start, Direction.Rotation(), Spawn);
    // ActorSpawned/BeginPlay会同步通知外部；返回后只复核旧施法，不沿用后继激活的数据。
    if (ActivationToken!=CastActivationToken || !IsActive() || !IsValid(Source) || GetAvatarActorFromActorInfo()!=Source ||
        !IsValid(Combat) || Combat->GetCombatAvatarGeneration()!=Shot.SourceAvatarGeneration || Combat->IsCombatDead())
    {
        if (Projectile) Projectile->Destroy();
        // Destroy同样能同步激活后继。只结束仍属于当前令牌的失效施法，不能取消后继实例。
        if (ActivationToken==CastActivationToken && IsActive())
            EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,true);
        return;
    }
    // Spawn通知可能销毁目标；冻结身份失效时取消，不能把被GC置空的目标当作最初未选择。
    Shot.Target=FrozenProjectileTarget.Get();
    if (bHadFrozenTarget && !IsValid(Shot.Target.Get()))
    {
        if (Projectile) Projectile->Destroy();
        if (CastActivationToken==ActivationToken && IsActive())
            EndAbility(CurrentSpecHandle,CurrentActorInfo,CurrentActivationInfo,true,true);
        return;
    }
    if (!Projectile || !Projectile->AuthorityInitialize(Shot, ShotMotion, Direction, StyleId))
    {
        if (Projectile) Projectile->Destroy();
        if (ActivationToken==CastActivationToken && IsActive())
            EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
        return;
    }
    if (ActivationToken!=CastActivationToken || !IsActive()) return;
    ++ReleasedProjectiles; --RemainingProjectiles;
    if (RemainingProjectiles == 0)
    { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); return; }
    // 单次timer逐枚排队；EndAbility在所有退出路径清除，避免循环timer与新激活竞争同一技能实例。
    World->GetTimerManager().SetTimer(BurstTimer,FTimerDelegate::CreateWeakLambda(this,[this,ActivationToken]()
        { if (CastActivationToken==ActivationToken) ReleaseNextProjectile(); }),IntervalSeconds,false);
}

void UDivineBeastsDevelopmentMagicAbility::EndAbility(FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    const FGuid EndedToken=CastActivationToken;
    CastActivationToken.Invalidate();
    if (AActor* Source = CastSpec.Source.Get(); IsValid(Source) && Source->GetWorld())
        Source->GetWorld()->GetTimerManager().ClearTimer(BurstTimer);
    BurstTimer.Invalidate(); FrozenProjectileTarget.Reset(); bHadFrozenTarget=false; RemainingProjectiles = 0; CastSpec = {}; StyleId = NAME_None;
    // 正常结束保留短回收动作到窗口自然过期；取消仅停止该次动作，不取消后继事实。
    if (bWasCancelled && ActorInfo && ActorInfo->AvatarActor.IsValid())
        if (auto* CastFacts=ActorInfo->AvatarActor->FindComponentByClass<UGamePlatformAbilityCastComponent>())
            CastFacts->CancelConfirmedCast(EndedToken);
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

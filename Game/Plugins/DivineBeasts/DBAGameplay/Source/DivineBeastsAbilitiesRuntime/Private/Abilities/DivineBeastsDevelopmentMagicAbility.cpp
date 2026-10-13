// 项目开发施法：数据与输入身份归项目，运动/碰撞/护盾/生命结算归平台，无Niagara客户端依赖。
#include "Abilities/DivineBeastsDevelopmentMagicAbility.h"
#include "Definitions/DivineBeastsDevelopmentMagicDefinition.h"
#include "Development/DivineBeastsDevelopmentAbilityPolicy.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"

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
    Motion = Definition->ProjectileMotion; StyleId = Definition->GetPresentationStyleId();
    RemainingProjectiles = Definition->ProjectileCount; ReleasedProjectiles = 0;
    IntervalSeconds = Definition->ProjectileIntervalSeconds;
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
    const FVector Direction = Source->GetActorForwardVector().GetSafeNormal();
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
    CastActivationToken.Invalidate();
    if (AActor* Source = CastSpec.Source.Get(); IsValid(Source) && Source->GetWorld())
        Source->GetWorld()->GetTimerManager().ClearTimer(BurstTimer);
    BurstTimer.Invalidate(); RemainingProjectiles = 0; CastSpec = {}; StyleId = NAME_None;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

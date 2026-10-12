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
    // 复用配置技能的Commit代次初始化，不能调用父开发样板的“立刻射线/结束”动作。
    UDivineBeastsConfiguredGameplayAbility::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    AActor* Avatar = GetAvatarActorFromActorInfo();
    auto* Combat = IsValid(Avatar) ? Avatar->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    const auto* Definition = Cast<UDivineBeastsDevelopmentMagicDefinition>(UAssetManager::Get().GetPrimaryAssetObject(AbilityDefinitionId));
    FDivineBeastsAbilityBalanceRow Balance; FString Error;
    if (!DivineBeasts::DevelopmentAbilities::IsEnabledForCurrentProcess() || !Avatar || !Avatar->HasAuthority() ||
        !Combat || Combat->IsCombatDead() || !Definition || !Definition->ValidateDefinition().IsSuccess() ||
        !TryReadConfiguredBalance(Balance, Error) || Balance.BaseDamage <= 0 ||
        !FMath::IsNearlyEqual(Balance.CastRangeCm, Definition->ProjectileMotion.RangeCm) ||
        !CommitAbility(Handle, ActorInfo, ActivationInfo))
    { EndAbility(Handle, ActorInfo, ActivationInfo, true, true); return; }
    // 原生GAS同步成本/冷却回调可能取消施法；不沿用Commit前资格生成投射物。
    if (!IsActive()) return;
    CastSpec = {}; CastSpec.Source = Avatar; CastSpec.Magnitude = Balance.BaseDamage;
    CastSpec.DamageType = Balance.DamageType; CastSpec.SourceAbilityId = Balance.AbilityId;
    CastSpec.SourceAvatarGeneration = Combat->GetCombatAvatarGeneration();
    Motion = Definition->ProjectileMotion; StyleId = Definition->GetPresentationStyleId();
    RemainingProjectiles = Definition->ProjectileCount; ReleasedProjectiles = 0;
    IntervalSeconds = Definition->ProjectileIntervalSeconds;
    ReleaseNextProjectile();
}

void UDivineBeastsDevelopmentMagicAbility::ReleaseNextProjectile()
{
    AActor* Source = CastSpec.Source.Get();
    auto* Combat = IsValid(Source) ? Source->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    UWorld* World = Source ? Source->GetWorld() : nullptr;
    if (!IsActive() || !IsValid(Source) || !Source->HasAuthority() || !World || World->bIsTearingDown ||
        !Combat || Combat->IsCombatDead() || Combat->GetCombatAvatarGeneration() != CastSpec.SourceAvatarGeneration || RemainingProjectiles <= 0)
    { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return; }
    const FVector Direction = Source->GetActorForwardVector().GetSafeNormal();
    const FVector Start = Source->GetActorLocation() + FVector(0, 0, 50) + Direction * 50;
    FActorSpawnParameters Spawn; Spawn.Owner = Source; Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Projectile = World->SpawnActor<AGamePlatformCombatProjectile>(Start, Direction.Rotation(), Spawn);
    FGamePlatformCombatSpec Shot = CastSpec; Shot.EventId = FGuid::NewGuid();
    FGamePlatformCombatProjectileMotion ShotMotion = Motion;
    if (Motion.HelixRadiusCm > 0) ShotMotion.InitialPhaseTurns = ReleasedProjectiles / 3.0f;
    if (!Projectile || !Projectile->AuthorityInitialize(Shot, ShotMotion, Direction, StyleId))
    {
        if (Projectile) Projectile->Destroy();
        EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true); return;
    }
    ++ReleasedProjectiles; --RemainingProjectiles;
    if (RemainingProjectiles == 0)
    { EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false); return; }
    // 单次timer逐枚排队；EndAbility在所有退出路径清除，避免循环timer与新激活竞争同一技能实例。
    World->GetTimerManager().SetTimer(BurstTimer, this, &ThisClass::ReleaseNextProjectile, IntervalSeconds, false);
}

void UDivineBeastsDevelopmentMagicAbility::EndAbility(FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo, FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    if (AActor* Source = CastSpec.Source.Get(); IsValid(Source) && Source->GetWorld())
        Source->GetWorld()->GetTimerManager().ClearTimer(BurstTimer);
    BurstTimer.Invalidate(); RemainingProjectiles = 0; CastSpec = {}; StyleId = NAME_None;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

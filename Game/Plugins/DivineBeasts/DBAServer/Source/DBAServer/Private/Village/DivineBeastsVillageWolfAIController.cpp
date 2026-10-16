// SSLM项目狼AI只承担中立Combat规格的可信映射；平台层仍负责感知/目标选择/移动。
#include "Village/DivineBeastsVillageWolfAIController.h"

#include "Characters/DivineBeastsVillageWolf.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerStart.h"
#include "Types/GamePlatformCombatSpec.h"

ADivineBeastsVillageWolfAIController::ADivineBeastsVillageWolfAIController()
{
    // 原生平台AI计时器是唯一决策驱动，不启用控制器逐帧Tick来扫描玩家。
    PrimaryActorTick.bCanEverTick = false;
}

bool ADivineBeastsVillageWolfAIController::TryExecuteNativeAttack(AActor& ValidatedTarget)
{
    auto* Wolf = Cast<ADivineBeastsVillageWolf>(GetPawn());
    auto* TargetPawn = Cast<APawn>(&ValidatedTarget);
    UWorld* World = GetWorld();
    if (!HasAuthority() || !IsValid(Wolf) || !Wolf->IsWolfAlive() || !Wolf->CanSourceCombat() ||
        !IsValid(TargetPawn) || !TargetPawn->IsPlayerControlled() || !World || World->bIsTearingDown ||
        TargetPawn->GetWorld() != World || TargetPawn->IsActorBeingDestroyed()) return false;

    auto* SourceCombat = Wolf->GetGamePlatformCombatComponent();
    auto* TargetCombat = TargetPawn->FindComponentByClass<UGamePlatformCombatComponent>();
    if (!IsValid(SourceCombat) || !IsValid(TargetCombat) || TargetCombat->IsCombatDead() ||
        SourceCombat->GetCombatAvatarGeneration() <= 0 || TargetCombat->GetCombatAvatarGeneration() <= 0)
        return false;

    constexpr float MeleeLimitCm = 185.0f;
    constexpr float PlayerSafeRadiusCm = 2000.0f;
    const FVector WolfLocation = Wolf->GetActorLocation();
    const FVector TargetLocation = TargetPawn->GetActorLocation();
    if (FVector::DistSquared(WolfLocation, TargetLocation) > FMath::Square(MeleeLimitCm))
        return false;

    // 新手保护：在任意实际PlayerStart周围20米内不可受到狼攻击。
    // 与散布层同样保守；即使怪物因位移进入新手安全区，也不能造成伤害。
    for (TActorIterator<APlayerStart> It(World); It; ++It)
    {
        if (!IsValid(*It) || It->IsActorBeingDestroyed()) continue;
        if (FVector::DistSquared2D(TargetLocation, It->GetActorLocation()) < FMath::Square(PlayerSafeRadiusCm))
            return false;
    }

    // 攻击只能命中无遮挡的近战目标。客户端上报的HitResult永远不参与权威结算。
    const FVector Start = WolfLocation + FVector(0.0f, 0.0f, 40.0f);
    const FVector End = TargetLocation + FVector(0.0f, 0.0f, 40.0f);
    FCollisionQueryParams Params(TEXT("VillageWolfMeleeAuthority"), false);
    Params.AddIgnoredActor(Wolf);
    FHitResult Hit;
    if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ||
        Hit.GetActor() != TargetPawn) return false;

    FGamePlatformCombatSpec Spec;
    Spec.EventId = FGuid::NewGuid();
    Spec.Source = Wolf;
    Spec.Target = TargetPawn;
    Spec.Magnitude = 12.0f; // 教学前期每次狼咬的基础物理伤害；后续从项目怪物Definition加载。
    Spec.DamageType = EGamePlatformDamageType::Physical;
    Spec.SourceAvatarGeneration = SourceCombat->GetCombatAvatarGeneration();
    Spec.TargetAvatarGeneration = TargetCombat->GetCombatAvatarGeneration();
    Spec.SourceAbilityId = FName(TEXT("VillageWolf.Bite"));
    Spec.HitContext.bHasValidatedHit = true;
    Spec.HitContext.TraceStart = Start;
    Spec.HitContext.TraceEnd = End;
    Spec.HitContext.ImpactPoint = Hit.ImpactPoint;
    Spec.HitContext.ImpactNormal = Hit.ImpactNormal.GetSafeNormal();
    Spec.HitContext.ValidatedDistance = FVector::Distance(Start, End);
    // 最终由平台统一核验角色身份、命中、护盾、伤害和死亡。失败不能声称成功。
    const bool bApplied = SourceCombat->ApplyDamage(Spec).IsSuccess();
    // 复用平台已有复制表现事实，不另建狼攻击RPC、动画播放器或客户端伤害时序。
    // 动画发布失败也不能回滚真实伤害；反伤/死亡重入后禁止再发布新咬击。
    if (bApplied && IsValid(Wolf) && Wolf->IsWolfAlive() && !Wolf->IsActorBeingDestroyed())
        if (auto* Cast = Wolf->FindComponentByClass<UGamePlatformAbilityCastComponent>())
            Cast->PublishConfirmedCast(Spec.EventId, Spec.SourceAbilityId, FName(TEXT("VillageWolf.Bite")),
                Spec.SourceAvatarGeneration, 0.8f);
    return bApplied;
}

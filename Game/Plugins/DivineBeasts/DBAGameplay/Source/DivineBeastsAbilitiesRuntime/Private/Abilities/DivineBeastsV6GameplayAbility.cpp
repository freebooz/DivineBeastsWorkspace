// SSLM V6服务器权威技能公共原语：任何未知历史机制在GAS扣费前拒绝，不以特效替代玩法。
#include "Abilities/DivineBeastsV6GameplayAbility.h"

#include "Catalog/DivineBeastsHeroCatalog.h"
#include "Characters/DivineBeastsCharacter.h"
#include "Components/DivineBeastsCharacterComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Components/GamePlatformAbilityCastComponent.h"
#include "Actors/GamePlatformCombatProjectile.h"
#include "Definitions/DivineBeastsAbilityDefinition.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Engine/AssetManager.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Actor.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "NavigationSystem.h"
#include "Interfaces/GamePlatformCombatant.h"
#include "CollisionQueryParams.h"

namespace
{
/** 最多一次命中／有限半径事件，避免未经审核的4段终极技能被同帧放大四次。 */
constexpr int32 MaxV6AffectedActors = 16;
constexpr float MaxV6RangeCm = 2000.0f;
constexpr float MaxV6AreaRadiusCm = 700.0f;
constexpr float MaxV6ShieldFraction = 0.5f;
constexpr float MaxV6HealingFraction = 0.5f;
constexpr float MaxV6ControlSeconds = 3.0f;

struct FV6TargetSnapshot
{
    TWeakObjectPtr<AActor> Actor;
    int32 AvatarGeneration = 0;
    FVector ImpactLocation = FVector::ZeroVector;
    FVector ImpactNormal = FVector::UpVector;
};

/** 源与目标同世界且未死亡、未改化身；队伍关系由项目已接入的服务器组合根判断。 */
bool IsCurrentTarget(AActor* Source, const ADivineBeastsCharacter* Hero,
    AActor* Target, int32 ExpectedGeneration, bool bRequireFriendly)
{
    if (!IsValid(Source) || !IsValid(Hero) || !IsValid(Target) ||
        Target->IsActorBeingDestroyed() || Target->GetWorld() != Source->GetWorld() ||
        Target->GetActorLocation().ContainsNaN())
    {
        return false;
    }
    const auto* Combat = Target->FindComponentByClass<UGamePlatformCombatComponent>();
    const auto* Policy = Cast<IGamePlatformCombatant>(Target);
    if (!IsValid(Combat) || Combat->IsCombatDead() ||
        Combat->GetCombatAvatarGeneration() != ExpectedGeneration ||
        (Policy && !Policy->CanReceiveCombat()))
    {
        return false;
    }
    const bool bFriendly = Hero->AuthorityIsFriendlyCombatTarget(Target);
    return bRequireFriendly ? bFriendly : !bFriendly && Target != Source;
}

FGamePlatformCombatSpec MakeV6Spec(AActor* Source, UGamePlatformCombatComponent* Combat,
    const FV6TargetSnapshot& Target, const FDivineBeastsAbilityBalanceRow& Balance, float Magnitude)
{
    FGamePlatformCombatSpec Spec;
    Spec.EventId = FGuid::NewGuid();
    Spec.Source = Source;
    Spec.Target = Target.Actor.Get();
    Spec.Magnitude = Magnitude;
    Spec.DamageType = Balance.DamageType;
    Spec.SourceAbilityId = Balance.AbilityId;
    Spec.SourceAvatarGeneration = Combat->GetCombatAvatarGeneration();
    Spec.TargetAvatarGeneration = Target.AvatarGeneration;
    Spec.HitContext.bHasValidatedHit = true;
    Spec.HitContext.TraceStart = Source->GetActorLocation();
    Spec.HitContext.TraceEnd = Target.ImpactLocation;
    Spec.HitContext.ImpactPoint = Target.ImpactLocation;
    Spec.HitContext.ImpactNormal = Target.ImpactNormal;
    Spec.HitContext.ValidatedDistance =
        FVector::Distance(Spec.HitContext.TraceStart, Spec.HitContext.ImpactPoint);
    return Spec;
}

/** 可见性测试属于服务器世界物理查询；无法确认阻挡时拒绝而非让AoE穿墙。 */
bool HasV6LineOfSight(UWorld* World, AActor* Source, AActor* Target)
{
    if (!World || !IsValid(Source) || !IsValid(Target)) return false;
    const FVector Start = Source->GetActorLocation() + FVector(0, 0, 45);
    const FVector End = Target->GetActorLocation() + FVector(0, 0, 45);
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SSLMV6LineOfSight), false, Source);
    FHitResult Hit;
    return !World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params) ||
        Hit.GetActor() == Target;
}

/** 当前客户端目标只提供意图；冻结后的服务器真实Actor及Avatar代次才可用于结算。 */
bool GetV6SelectedTarget(AActor* Source, UGamePlatformCombatComponent* Combat,
    float RangeCm, FV6TargetSnapshot& Out)
{
    Out = {};
    int32 Generation = 0;
    AActor* Target = IsValid(Combat) ? Combat->GetSelectedProjectileTarget(RangeCm, Generation) : nullptr;
    if (!IsValid(Target) || Generation <= 0 || Target->GetWorld() != Source->GetWorld() ||
        !HasV6LineOfSight(Source->GetWorld(), Source, Target))
    {
        return false;
    }
    Out.Actor = Target;
    Out.AvatarGeneration = Generation;
    if (auto* TargetCombat = Target->FindComponentByClass<UGamePlatformCombatComponent>())
        Out.ImpactLocation = TargetCombat->GetCombatTargetPoint();
    else
        Out.ImpactLocation = Target->GetActorLocation();
    Out.ImpactNormal = (Source->GetActorLocation() - Out.ImpactLocation).GetSafeNormal();
    return !Out.ImpactLocation.ContainsNaN();
}


/** 在Commit后的最新服务器世界重新核对实际施法距离，不能沿用提交前被冻结的旧距离。 */
bool IsV6WithinRange(const AActor* Source, const AActor* Target, float MaxRangeCm)
{
    if (!IsValid(Source) || !IsValid(Target) || !FMath::IsFinite(MaxRangeCm) ||
        MaxRangeCm < 0.0f || Source->GetWorld() != Target->GetWorld() ||
        Source->GetActorLocation().ContainsNaN() || Target->GetActorLocation().ContainsNaN())
        return false;
    return FVector::DistSquared(Source->GetActorLocation(), Target->GetActorLocation()) <=
        FMath::Square(MaxRangeCm);
}

/** 区域结算永久绑定施法时的服务器范围中心；不能让Commit回调中的移动迁移整片AOE。 */
bool IsV6AreaCenterValid(const AActor* Source, const FVector& Center, float MaxCastRangeCm)
{
    if (!IsValid(Source) || Source->GetActorLocation().ContainsNaN() ||
        Center.ContainsNaN() || !FMath::IsFinite(MaxCastRangeCm) || MaxCastRangeCm < 0.0f)
        return false;
    // 自身周围零射程技能，角色在提交期间改变位置时拒绝按旧位置产生新的区域伤害。
    const float Radius = MaxCastRangeCm > 0.0f ? MaxCastRangeCm : 1.0f;
    return FVector::DistSquared(Source->GetActorLocation(), Center) <= FMath::Square(Radius);
}

/** 普攻前向射线同时检查Pawn和障挡物，友军在前方时不得击穿队友。 */
bool TraceV6ForwardEnemy(AActor* Source, const ADivineBeastsCharacter* Hero,
    float RangeCm, FV6TargetSnapshot& Out)
{
    Out = {};
    UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
    if (!World || Source->GetActorLocation().ContainsNaN()) return false;
    const FVector Start = Source->GetActorLocation() + FVector(0, 0, 50);
    const FVector Direction = Source->GetActorForwardVector().GetSafeNormal();
    if (Direction.IsNearlyZero()) return false;
    const FVector End = Start + Direction * RangeCm;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SSLMV6ForwardTrace), false, Source);
    FHitResult VisibilityHit, PawnHit;
    const bool bVis = World->LineTraceSingleByChannel(
        VisibilityHit, Start, End, ECC_Visibility, Params);
    const bool bPawn = World->LineTraceSingleByObjectType(
        PawnHit, Start, End, FCollisionObjectQueryParams(ECC_Pawn), Params);
    const FHitResult* Hit = bPawn && (!bVis || PawnHit.Distance < VisibilityHit.Distance)
        ? &PawnHit : (bVis ? &VisibilityHit : nullptr);
    if (!Hit || !Hit->bBlockingHit) return false;
    AActor* Target = Hit->GetActor();
    const auto* Combat = IsValid(Target)
        ? Target->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    if (!Combat || !IsCurrentTarget(Source, Hero, Target,
            Combat->GetCombatAvatarGeneration(), false))
        return false;
    Out.Actor = Target;
    Out.AvatarGeneration = Combat->GetCombatAvatarGeneration();
    Out.ImpactLocation = Hit->ImpactPoint;
    Out.ImpactNormal = Hit->ImpactNormal.GetSafeNormal();
    return !Out.ImpactLocation.ContainsNaN();
}

/** 以冻结的选中敌人为范围中心；不信任客户端地面坐标和视觉Niagara大小。 */
bool BuildV6AreaTargets(AActor* Source, const ADivineBeastsCharacter* Hero,
    UGamePlatformCombatComponent* SourceCombat, const FDivineBeastsAbilityBalanceRow& Balance,
    TArray<FV6TargetSnapshot>& Out, FVector& OutFrozenAreaCenter)
{
    Out.Reset();
    OutFrozenAreaCenter = FVector::ZeroVector;
    UWorld* World = IsValid(Source) ? Source->GetWorld() : nullptr;
    if (!World || Balance.AreaRadiusCm <= 0.0f || Balance.AreaRadiusCm > MaxV6AreaRadiusCm)
        return false;
    FVector Center = Source->GetActorLocation();
    if (Balance.CastRangeCm > 0.0f)
    {
        FV6TargetSnapshot Chosen;
        if (!GetV6SelectedTarget(Source, SourceCombat, Balance.CastRangeCm, Chosen))
            return false;
        AActor* Actor = Chosen.Actor.Get();
        if (!IsCurrentTarget(Source, Hero, Actor, Chosen.AvatarGeneration, false))
            return false;
        Center = Actor->GetActorLocation();
    }

    if (!IsV6AreaCenterValid(Source, Center, Balance.CastRangeCm)) return false;
    OutFrozenAreaCenter = Center;
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(SSLMV6AreaOverlap), false, Source);
    World->OverlapMultiByObjectType(Overlaps, Center, FQuat::Identity,
        FCollisionObjectQueryParams(ECC_Pawn),
        FCollisionShape::MakeSphere(Balance.AreaRadiusCm), Params);
    TSet<AActor*> Unique;
    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* Target = Overlap.GetActor();
        if (Unique.Contains(Target) || !IsValid(Target)) continue;
        auto* TargetCombat = Target->FindComponentByClass<UGamePlatformCombatComponent>();
        if (!TargetCombat || !IsCurrentTarget(Source, Hero, Target,
                TargetCombat->GetCombatAvatarGeneration(), false) ||
            !HasV6LineOfSight(World, Source, Target))
            continue;
        if (Out.Num() >= MaxV6AffectedActors) return false; // 目标过多不随机截断/不产生部分伤害。
        Unique.Add(Target);
        FV6TargetSnapshot& Snapshot = Out.AddDefaulted_GetRef();
        Snapshot.Actor = Target;
        Snapshot.AvatarGeneration = TargetCombat->GetCombatAvatarGeneration();
        Snapshot.ImpactLocation = TargetCombat->GetCombatTargetPoint();
        Snapshot.ImpactNormal =
            (Source->GetActorLocation() - Snapshot.ImpactLocation).GetSafeNormal();
    }
    return !Out.IsEmpty();
}

/** 短距离权威地面位移：导航落点和全胶囊扫掠共同约束，拒绝穿墙和非导航区域。 */
bool PrepareV6SafeDash(ADivineBeastsCharacter* Hero, float Cm, FVector& Out)
{
    Out = FVector::ZeroVector;
    if (!IsValid(Hero) || !Hero->HasAuthority() || !FMath::IsFinite(Cm) || Cm <= 0 || Cm > 950) return false;
    UWorld* World = Hero->GetWorld();
    const UCapsuleComponent* Capsule = Hero->GetCapsuleComponent();
    const UCharacterMovementComponent* Movement = Hero->GetCharacterMovement();
    if (!World || World->bIsTearingDown || !Capsule || !Movement || !Movement->IsMovingOnGround()) return false;
    const FVector Start = Hero->GetActorLocation(), Direction = Hero->GetActorForwardVector().GetSafeNormal2D();
    if (Start.ContainsNaN() || Direction.IsNearlyZero()) return false;
    const FVector Candidate = Start + Direction * Cm;
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    FNavLocation Location;
    if (!Nav || !Nav->ProjectPointToNavigation(Candidate, Location, FVector(45, 45, 170)) ||
        FVector::DistSquared2D(Location.Location, Candidate) > FMath::Square(45.f)) return false;
    const FVector Destination = Location.Location + FVector(0, 0, Capsule->GetScaledCapsuleHalfHeight());
    if (Destination.ContainsNaN() || FMath::Abs(Destination.Z - Start.Z) > 160.f) return false;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(SSLMV6DashSweep), false, Hero);
    FHitResult Hit;
    if (World->SweepSingleByChannel(Hit, Start, Destination, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeCapsule(Capsule->GetScaledCapsuleRadius(),
            Capsule->GetScaledCapsuleHalfHeight()), Query) && Hit.bBlockingHit) return false;
    Out = Destination;
    return true;
}
}

UDivineBeastsV6GameplayAbility::UDivineBeastsV6GameplayAbility()
{
    NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

const FGameplayTagContainer* UDivineBeastsV6GameplayAbility::GetCooldownTags() const
{
    return &ApprovedCooldownTags;
}

bool UDivineBeastsV6GameplayAbility::IsValidDamageControlAction(
    const FDivineBeastsEightSlotDraftAction& ControlAction, bool bSilence)
{
    const bool bExactControl = bSilence
        ? ControlAction.ModifierId == TEXT("silence")
        : ControlAction.ModifierId == TEXT("stun");
    return ControlAction.ActionKindId == TEXT("control") &&
        bExactControl &&
        ControlAction.HitCount == 1 &&
        FMath::IsNearlyZero(ControlAction.Magnitude) &&
        FMath::IsFinite(ControlAction.DurationSeconds) &&
        ControlAction.DurationSeconds > 0.0f &&
        ControlAction.DurationSeconds <= MaxV6ControlSeconds;
}


bool UDivineBeastsV6GameplayAbility::IsValidDamageSlowAction(
    const FDivineBeastsEightSlotDraftAction& ControlAction)
{
    return ControlAction.ActionKindId == TEXT("control") &&
        ControlAction.ModifierId == TEXT("slow") &&
        ControlAction.HitCount == 1 &&
        FMath::IsFinite(ControlAction.Magnitude) &&
        ControlAction.Magnitude > 0.0f &&
        ControlAction.Magnitude <= 0.70f &&
        FMath::IsFinite(ControlAction.DurationSeconds) &&
        ControlAction.DurationSeconds > 0.0f &&
        ControlAction.DurationSeconds <= MaxV6ControlSeconds;
}

bool UDivineBeastsV6GameplayAbility::ValidateNativeContract(
    const UDivineBeastsAbilityDefinition& Definition,
    const FDivineBeastsAbilityBalanceRow& Balance, FString& OutError) const
{
    OutError.Reset();
    const bool bDamageControlMode =
        ExecutionMode == EDivineBeastsV6NativeMode::DamageAndStun ||
        ExecutionMode == EDivineBeastsV6NativeMode::DamageAndSilence ||
        ExecutionMode == EDivineBeastsV6NativeMode::RadialDamageAndStun ||
        ExecutionMode == EDivineBeastsV6NativeMode::DamageAndSlow;
    if (!Definition.bV6NativeApproved || Definition.bEightSlotDraftOnly ||
        !Definition.ValidateDefinition().IsSuccess() ||
        ExecutionMode == EDivineBeastsV6NativeMode::Unsupported ||
        Definition.DraftActions.Num() != (bDamageControlMode ? 2 : 1) ||
        !Balance.Validate(OutError))
    {
        OutError = TEXT("V6技能尚未批准，或需要多行为/分段等未实现的服务器机制。");
        return false;
    }
    const FDivineBeastsEightSlotDraftAction& Action = Definition.DraftActions[0];
    const bool bHasLegalRange = Balance.CastRangeCm >= 0.0f &&
        Balance.CastRangeCm <= MaxV6RangeCm;
    const bool bSingleHit = Action.HitCount == 1;
    if (bDamageControlMode)
    {
        // 只允许一次确定性伤害和一种有限时控制；Slow单独走原生MovementSpeedMultiplier属性。
        const FDivineBeastsEightSlotDraftAction& ControlAction = Definition.DraftActions[1];
        const bool bSilence = ExecutionMode == EDivineBeastsV6NativeMode::DamageAndSilence;
        const bool bControlValid = ExecutionMode == EDivineBeastsV6NativeMode::DamageAndSlow
            ? IsValidDamageSlowAction(ControlAction)
            : IsValidDamageControlAction(ControlAction, bSilence);
        if (!bControlValid)
        {
            OutError = TEXT("V6伤害与控制动作不匹配：减速、眩晕、沉默不能互相替代，其他历史机制需独立实现。");
            return false;
        }
    }

    const auto MagnitudeMatchesDamage = [&]()
    {
        return Action.ActionKindId == TEXT("damage") && bSingleHit &&
            Balance.BaseDamage > 0.0f &&
            FMath::IsNearlyEqual(Action.Magnitude, Balance.BaseDamage, 0.01f);
    };
    bool bAllowed = false;
    switch (ExecutionMode)
    {
    case EDivineBeastsV6NativeMode::ForwardDamage:
        bAllowed = bHasLegalRange && Balance.CastRangeCm > 0 &&
            FMath::IsNearlyZero(Balance.AreaRadiusCm) && MagnitudeMatchesDamage() &&
            Definition.DraftTargetModeId == TEXT("enemy");
        break;
    case EDivineBeastsV6NativeMode::SingleProjectile:
        bAllowed = bHasLegalRange && Balance.CastRangeCm > 0 &&
            FMath::IsNearlyZero(Balance.AreaRadiusCm) && MagnitudeMatchesDamage() &&
            ProjectileMotion.IsValid() &&
            FMath::IsNearlyEqual(ProjectileMotion.RangeCm, Balance.CastRangeCm, 0.01f) &&
            Definition.DraftTargetModeId == TEXT("enemy");
        break;
    case EDivineBeastsV6NativeMode::RadialDamage:
        bAllowed = bHasLegalRange &&
            Balance.AreaRadiusCm > 0 && Balance.AreaRadiusCm <= MaxV6AreaRadiusCm &&
            MagnitudeMatchesDamage() &&
            // 同时含伤害和治疗的范围技能必须执行两条权威结果；单一Damage不得冒充全技能。
            Definition.DraftTargetModeId == TEXT("area_or_target");
        break;
    case EDivineBeastsV6NativeMode::DamageAndSlow:
    case EDivineBeastsV6NativeMode::DamageAndStun:
    case EDivineBeastsV6NativeMode::DamageAndSilence:
        bAllowed = bHasLegalRange && Balance.CastRangeCm > 0.0f &&
            FMath::IsNearlyZero(Balance.AreaRadiusCm) && MagnitudeMatchesDamage() &&
            Definition.DraftTargetModeId == TEXT("enemy");
        break;
    case EDivineBeastsV6NativeMode::RadialDamageAndStun:
        bAllowed = bHasLegalRange && Balance.AreaRadiusCm > 0.0f &&
            Balance.AreaRadiusCm <= MaxV6AreaRadiusCm && MagnitudeMatchesDamage() &&
            Definition.DraftTargetModeId == TEXT("area_or_target");
        break;
    case EDivineBeastsV6NativeMode::SingleHealing:
        bAllowed = bHasLegalRange && Balance.CastRangeCm > 0 &&
            Balance.BaseDamage == 0.0f && Action.ActionKindId == TEXT("healing") &&
            Action.Magnitude > 0 && Action.Magnitude <= MaxV6HealingFraction &&
            Definition.DraftTargetModeId == TEXT("friendly_or_self");
        break;
    case EDivineBeastsV6NativeMode::SingleShield:
        bAllowed = bHasLegalRange && Balance.BaseDamage == 0.0f &&
            Action.ActionKindId == TEXT("shield") &&
            Action.Magnitude > 0 && Action.Magnitude <= MaxV6ShieldFraction &&
            Action.DurationSeconds > 0 && Action.DurationSeconds <= 10.0f &&
            (Definition.DraftTargetModeId == TEXT("friendly_or_self") ||
             Definition.DraftTargetModeId == TEXT("self"));
        break;
    case EDivineBeastsV6NativeMode::SafeDash:
        bAllowed = bHasLegalRange && Balance.BaseDamage == 0.0f &&
            FMath::IsNearlyZero(Balance.AreaRadiusCm) &&
            Action.ActionKindId == TEXT("movement") && Action.HitCount == 1 &&
            Action.Magnitude > 0 && Action.Magnitude <= 950.0f &&
            Balance.CastRangeCm >= Action.Magnitude &&
            Definition.DraftTargetModeId == TEXT("movement_direction");
        break;
    case EDivineBeastsV6NativeMode::SingleStun:
    case EDivineBeastsV6NativeMode::SingleSilence:
        bAllowed = bHasLegalRange && Balance.CastRangeCm > 0 &&
            Balance.BaseDamage == 0.0f && Action.ActionKindId == TEXT("control") &&
            Action.ModifierId == (ExecutionMode == EDivineBeastsV6NativeMode::SingleStun
                ? TEXT("stun") : TEXT("silence")) &&
            Action.DurationSeconds > 0 && Action.DurationSeconds <= MaxV6ControlSeconds &&
            Definition.DraftTargetModeId == TEXT("enemy");
        break;
    default:
        bAllowed = false;
        break;
    }
    if (!bAllowed)
    {
        OutError = TEXT("V6数据与可执行原语不一致：多段、减速、真实位移、印记联动或复合机制须独立实现。");
        return false;
    }
    if (Balance.CooldownSeconds > 0.0f && ApprovedCooldownTags.IsEmpty())
    {
        OutError = TEXT("已批准V6技能缺少GAS冷却标签，不能授权。");
        return false;
    }
    OutError.Reset();
    return true;
}

bool UDivineBeastsV6GameplayAbility::IsCurrentActivation(
    const FGuid& ExpectedToken, const AActor* ExpectedAvatar,
    const UGamePlatformCombatComponent* ExpectedCombat, int32 ExpectedGeneration) const
{
    return ExpectedToken.IsValid() && ActivationToken == ExpectedToken && IsActive() &&
        IsValid(ExpectedAvatar) && !ExpectedAvatar->IsActorBeingDestroyed() &&
        ExpectedAvatar->HasAuthority() &&
        GetAvatarActorFromActorInfo() == ExpectedAvatar &&
        IsValid(ExpectedCombat) && !ExpectedCombat->IsCombatDead() &&
        ExpectedCombat->GetCombatAvatarGeneration() == ExpectedGeneration &&
        IsValid(ExpectedAvatar->GetWorld()) &&
        !ExpectedAvatar->GetWorld()->bIsTearingDown;
}

void UDivineBeastsV6GameplayAbility::ActivateAbility(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    const FGuid Token = FGuid::NewGuid();
    ActivationToken = Token;
    Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
    if (!IsActive() || ActivationToken != Token) return;

    AActor* Source = GetAvatarActorFromActorInfo();
    auto* Hero = Cast<ADivineBeastsCharacter>(Source);
    auto* Combat = IsValid(Source) ? Source->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    const int32 SourceGeneration = IsValid(Combat) ? Combat->GetCombatAvatarGeneration() : 0;
    FDivineBeastsAbilityBalanceRow Balance;
    FString Error;
    const UDivineBeastsAbilityDefinition* Definition =
        AbilityDefinitionId.IsValid() ? Cast<UDivineBeastsAbilityDefinition>(
            UAssetManager::Get().GetPrimaryAssetObject(AbilityDefinitionId)) : nullptr;
    if (!Hero || !Combat || SourceGeneration <= 0 ||
        !IsCurrentActivation(Token, Source, Combat, SourceGeneration) ||
        !TryReadConfiguredBalance(Balance, Error) || !Definition ||
        !ValidateNativeContract(*Definition, Balance, Error))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }
    const FDivineBeastsEightSlotDraftAction& Action = Definition->DraftActions[0];
    FV6TargetSnapshot SingleTarget;
    TArray<FV6TargetSnapshot> AreaTargets;
    FVector FrozenAreaCenter = FVector::ZeroVector;
    bool bProjectileHadFrozenTarget = false;
    bool bValidTarget = false;
    if (ExecutionMode == EDivineBeastsV6NativeMode::ForwardDamage)
        bValidTarget = TraceV6ForwardEnemy(Source, Hero, Balance.CastRangeCm, SingleTarget);
    else if (ExecutionMode == EDivineBeastsV6NativeMode::SingleProjectile)
    {
        // 无选择时沿服务器角色朝向直射，有冻结的已选目标则服务器复核其身份与可见性。
        TWeakObjectPtr<AActor> IntentTarget;
        int32 IntentSourceGeneration = 0, IntentTargetGeneration = 0;
        const bool bHasIntent = Combat->GetSelectedProjectileTargetIntent(
            IntentTarget, IntentSourceGeneration, IntentTargetGeneration);
        bProjectileHadFrozenTarget = bHasIntent;
        bValidTarget = !bHasIntent ? !Source->GetActorForwardVector().GetSafeNormal().IsNearlyZero() :
            (IntentSourceGeneration == SourceGeneration &&
             GetV6SelectedTarget(Source, Combat, Balance.CastRangeCm, SingleTarget) &&
             IntentTarget.Get() == SingleTarget.Actor.Get() &&
             IsCurrentTarget(Source, Hero, SingleTarget.Actor.Get(),
                 SingleTarget.AvatarGeneration, false));
    }
    else if (ExecutionMode == EDivineBeastsV6NativeMode::RadialDamage ||
             ExecutionMode == EDivineBeastsV6NativeMode::RadialDamageAndStun)
        bValidTarget = BuildV6AreaTargets(Source, Hero, Combat, Balance, AreaTargets, FrozenAreaCenter);
    else if (ExecutionMode == EDivineBeastsV6NativeMode::SafeDash)
    {
        FVector Destination;
        bValidTarget = PrepareV6SafeDash(Hero, Action.Magnitude, Destination);
    }
    else
    {
        // 选择过期、死亡或敌方目标不能悄悄降级成自疗／自盾，必须重新显式选中。
        TWeakObjectPtr<AActor> IntentActor;
        int32 IntentSourceGeneration = 0, IntentTargetGeneration = 0;
        const bool bSelected = Combat->GetSelectedProjectileTargetIntent(
            IntentActor, IntentSourceGeneration, IntentTargetGeneration);
        const bool bWantsFriendly =
            ExecutionMode == EDivineBeastsV6NativeMode::SingleHealing ||
            ExecutionMode == EDivineBeastsV6NativeMode::SingleShield;
        if (bWantsFriendly && Definition->DraftTargetModeId == TEXT("self"))
        {
            SingleTarget.Actor = Source;
            SingleTarget.AvatarGeneration = SourceGeneration;
            SingleTarget.ImpactLocation = Source->GetActorLocation();
            bValidTarget = true;
        }
        else if (bSelected)
        {
            bValidTarget = IntentSourceGeneration == SourceGeneration &&
                GetV6SelectedTarget(Source, Combat, Balance.CastRangeCm, SingleTarget) &&
                IntentActor.Get() == SingleTarget.Actor.Get();
        }
        else if (bWantsFriendly && Definition->DraftTargetModeId != TEXT("enemy"))
        {
            SingleTarget.Actor = Source;
            SingleTarget.AvatarGeneration = SourceGeneration;
            SingleTarget.ImpactLocation = Source->GetActorLocation();
            bValidTarget = true;
        }
        AActor* Target = SingleTarget.Actor.Get();
        bValidTarget = bValidTarget && IsCurrentTarget(
            Source, Hero, Target, SingleTarget.AvatarGeneration, bWantsFriendly);
    }
    if (!bValidTarget)
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    // Commit的同步回调可能让后继角色接管ASC，提交后重新检查化身身份，不做假退款。
    const bool bCommitted = CommitAbility(Handle, ActorInfo, ActivationInfo);
    if (ActivationToken != Token || !IsActive()) return;
    if (!bCommitted || !IsCurrentActivation(Token, Source, Combat, SourceGeneration))
    {
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    bool bSuccess = false;
    auto ApplyToTarget = [&](const FV6TargetSnapshot& Snapshot) -> bool
    {
        AActor* Target = Snapshot.Actor.Get();
        const bool bFriendly = ExecutionMode == EDivineBeastsV6NativeMode::SingleHealing ||
            ExecutionMode == EDivineBeastsV6NativeMode::SingleShield;
        if (!IsCurrentActivation(Token, Source, Combat, SourceGeneration) ||
            !IsCurrentTarget(Source, Hero, Target, Snapshot.AvatarGeneration, bFriendly))
            return false;
        // 实时重核：普通技能按当前角色/目标距离，范围技能按提交前冻结的中心和原始半径。
        if (ExecutionMode == EDivineBeastsV6NativeMode::RadialDamage ||
            ExecutionMode == EDivineBeastsV6NativeMode::RadialDamageAndStun)
        {
            if (!IsV6AreaCenterValid(Source, FrozenAreaCenter, Balance.CastRangeCm) ||
                Target->GetActorLocation().ContainsNaN() ||
                FVector::DistSquared(Target->GetActorLocation(), FrozenAreaCenter) >
                    FMath::Square(Balance.AreaRadiusCm))
                return false;
        }
        else if (!IsV6WithinRange(Source, Target, Balance.CastRangeCm))
            return false;
        if (Target != Source && !HasV6LineOfSight(Source->GetWorld(), Source, Target))
            return false;
        auto* TargetCombat = Target->FindComponentByClass<UGamePlatformCombatComponent>();
        if (!IsValid(TargetCombat)) return false;

        FV6TargetSnapshot Actual = Snapshot;
        if (ExecutionMode == EDivineBeastsV6NativeMode::ForwardDamage)
        {
            // 瞬时前向攻击必须再次击中同一Pawn；Commit回调里走开或被遮挡，不允许旧射线造成伤害。
            FV6TargetSnapshot UpdatedHit;
            if (!TraceV6ForwardEnemy(Source, Hero, Balance.CastRangeCm, UpdatedHit) ||
                UpdatedHit.Actor.Get() != Target ||
                UpdatedHit.AvatarGeneration != Snapshot.AvatarGeneration)
                return false;
            Actual = UpdatedHit;
        }
        else
        {
            // 非射线效果刷新真实结算点，不能用目标提交前的旧Impact伪造当前命中事实。
            Actual.ImpactLocation = TargetCombat->GetCombatTargetPoint();
            Actual.ImpactNormal = (Source->GetActorLocation() - Actual.ImpactLocation).GetSafeNormal();
            if (Actual.ImpactLocation.ContainsNaN() || Actual.ImpactNormal.ContainsNaN())
                return false;
        }
        float Magnitude = Balance.BaseDamage;
        if (ExecutionMode == EDivineBeastsV6NativeMode::SingleHealing ||
            ExecutionMode == EDivineBeastsV6NativeMode::SingleShield)
            Magnitude = TargetCombat->GetCombatMaxHealth() * Action.Magnitude;
        else if (ExecutionMode == EDivineBeastsV6NativeMode::SingleStun ||
                 ExecutionMode == EDivineBeastsV6NativeMode::SingleSilence)
            Magnitude = Action.DurationSeconds;
        if (!FMath::IsFinite(Magnitude) || Magnitude <= 0) return false;

        FGamePlatformCombatSpec Spec = MakeV6Spec(Source, Combat, Actual, Balance, Magnitude);
        FGamePlatformCombatResult Result;
        switch (ExecutionMode)
        {
        case EDivineBeastsV6NativeMode::ForwardDamage:
        case EDivineBeastsV6NativeMode::RadialDamage:
            Result = Combat->ApplyDamage(Spec); break;
        case EDivineBeastsV6NativeMode::DamageAndSlow:
        case EDivineBeastsV6NativeMode::DamageAndStun:
        case EDivineBeastsV6NativeMode::DamageAndSilence:
        case EDivineBeastsV6NativeMode::RadialDamageAndStun:
        {
            // 跨两次GameplayEffect不是原子事务；必须先有真实伤害，再独立请求控制。
            // 如伤害触发死亡、复活或换Pawn，不得使用已经失效的源/目标继续施控。
            const FGamePlatformCombatResult DamageResult = Combat->ApplyDamage(Spec);
            if (!DamageResult.IsSuccess()) return false;
            if (!IsCurrentActivation(Token, Source, Combat, SourceGeneration) ||
                !IsCurrentTarget(Source, Hero, Target, Snapshot.AvatarGeneration, false))
                return true; // 伤害已经执行，后续控制失败不得伪造全技能未命中

            const bool bAreaControl =
                ExecutionMode == EDivineBeastsV6NativeMode::RadialDamageAndStun;
            const bool bStillInRange = bAreaControl
                ? (IsV6AreaCenterValid(Source, FrozenAreaCenter, Balance.CastRangeCm) &&
                   !Target->GetActorLocation().ContainsNaN() &&
                   FVector::DistSquared(Target->GetActorLocation(), FrozenAreaCenter) <=
                       FMath::Square(Balance.AreaRadiusCm))
                : IsV6WithinRange(Source, Target, Balance.CastRangeCm);
            if (!bStillInRange || !HasV6LineOfSight(Source->GetWorld(), Source, Target) ||
                !IsValid(TargetCombat) ||
                Target->FindComponentByClass<UGamePlatformCombatComponent>() != TargetCombat)
                return true; // 目标或战斗组件被同步回调更换：保留已生效伤害，但不使用旧组件施控

            const FDivineBeastsEightSlotDraftAction& ControlAction = Definition->DraftActions[1];
            FV6TargetSnapshot CurrentHit = Actual;
            CurrentHit.ImpactLocation = TargetCombat->GetCombatTargetPoint();
            CurrentHit.ImpactNormal =
                (Source->GetActorLocation() - CurrentHit.ImpactLocation).GetSafeNormal();
            if (CurrentHit.ImpactLocation.ContainsNaN() || CurrentHit.ImpactNormal.ContainsNaN())
                return true; // 结算点丢失：伤害保留、控制跳过
            FGamePlatformCombatSpec ControlSpec = MakeV6Spec(
                Source, Combat, CurrentHit, Balance, ControlAction.DurationSeconds);
            ControlSpec.EventId = FGuid::NewGuid();
            ControlSpec.SourceAbilityId = Balance.AbilityId;
            ControlSpec.Magnitude = ControlAction.DurationSeconds;
            const FGamePlatformCombatResult ControlResult =
                ExecutionMode == EDivineBeastsV6NativeMode::DamageAndSlow
                    ? Combat->ApplySlow(ControlSpec, ControlAction.Magnitude, ControlAction.DurationSeconds)
                    : Combat->ApplyControl(
                        ControlSpec,
                        ExecutionMode == EDivineBeastsV6NativeMode::DamageAndSilence
                            ? EGamePlatformControlType::Silence : EGamePlatformControlType::Stun,
                        ControlAction.DurationSeconds);
            // ControlResult经平台独立事件回报，控制失败或目标死亡不能撤销已提交的真实伤害。
            (void)ControlResult;
            return true; // 伤害已经执行，后续控制失败不得伪造全技能未命中
        }
        case EDivineBeastsV6NativeMode::SingleHealing:
            Result = Combat->ApplyHealing(Spec); break;
        case EDivineBeastsV6NativeMode::SingleShield:
            Result = Combat->ApplyShield(Spec, Action.DurationSeconds); break;
        case EDivineBeastsV6NativeMode::SingleStun:
            Result = Combat->ApplyControl(Spec, EGamePlatformControlType::Stun, Action.DurationSeconds); break;
        case EDivineBeastsV6NativeMode::SingleSilence:
            Result = Combat->ApplyControl(Spec, EGamePlatformControlType::Silence, Action.DurationSeconds); break;
        default:
            return false;
        }
        if (!Result.IsSuccess()) return false;
        // 普攻每次服务器成功结算才可获得一次气势，拒绝按触发次数或动画窗口重复发放。
        if (ExecutionMode == EDivineBeastsV6NativeMode::ForwardDamage &&
            Definition->Kind == EDivineBeastsAbilityKind::Primary)
            if (auto* Identity = Source->FindComponentByClass<UDivineBeastsCharacterComponent>())
                Identity->AuthorityAwardMomentumForConfirmedBasicHit(Result, SourceGeneration);
        return true;
    };

    if (ExecutionMode == EDivineBeastsV6NativeMode::SingleProjectile)
    {
        AActor* FrozenTarget = SingleTarget.Actor.Get();
        const FVector Origin = Source->GetActorLocation() + FVector(0, 0, 50);
        // 已选择目标在Commit中被删除时，禁止悄悄回退为朝正前方的无目标弹体。
        const bool bCurrentFrozenTarget = bProjectileHadFrozenTarget &&
            IsCurrentTarget(Source, Hero, FrozenTarget, SingleTarget.AvatarGeneration, false) &&
            IsV6WithinRange(Source, FrozenTarget, Balance.CastRangeCm) &&
            HasV6LineOfSight(Source->GetWorld(), Source, FrozenTarget);
        const auto* TargetCombat = bCurrentFrozenTarget
            ? FrozenTarget->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
        // 无目标时保持服务器当前朝向；有目标时只使用提交后复核的当前命中点。
        const FVector FreshAim = IsValid(TargetCombat)
            ? TargetCombat->GetCombatTargetPoint() : FVector::ZeroVector;
        const FVector Direction = bProjectileHadFrozenTarget
            ? (FreshAim - Origin).GetSafeNormal()
            : Source->GetActorForwardVector().GetSafeNormal();
        if (!Direction.IsNearlyZero() &&
            IsCurrentActivation(Token, Source, Combat, SourceGeneration) &&
            (!bProjectileHadFrozenTarget ||
             (bCurrentFrozenTarget && IsValid(TargetCombat) && !FreshAim.ContainsNaN())))
        {
            FGamePlatformCombatSpec Shot;
            Shot.EventId = FGuid::NewGuid();
            Shot.Source = Source;
            Shot.Target = FrozenTarget;
            Shot.Magnitude = Balance.BaseDamage;
            Shot.DamageType = Balance.DamageType;
            Shot.SourceAbilityId = Balance.AbilityId;
            Shot.SourceAvatarGeneration = SourceGeneration;
            Shot.TargetAvatarGeneration = IsValid(FrozenTarget) ? SingleTarget.AvatarGeneration : 0;
            FActorSpawnParameters Spawn;
            Spawn.Owner = Source;
            Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            const FVector Start = Origin + Direction * 50.f;
            auto* Projectile = Source->GetWorld()->SpawnActor<AGamePlatformCombatProjectile>(
                Start, Direction.Rotation(), Spawn);
            if (Projectile && IsCurrentActivation(Token, Source, Combat, SourceGeneration) &&
                Projectile->AuthorityInitialize(Shot, ProjectileMotion, Direction,
                    Definition->DraftPresentationSemanticId))
                bSuccess = true;
            else if (IsValid(Projectile))
                Projectile->Destroy();
        }
    }
    else if (ExecutionMode == EDivineBeastsV6NativeMode::SafeDash)
    {
        // Commit后复查导航和障碍，动态场景碰撞变化导致失败不伪造成本退款。
        FVector Destination;
        FHitResult MoveHit;
        if (IsCurrentActivation(Token, Source, Combat, SourceGeneration) &&
            PrepareV6SafeDash(Hero, Action.Magnitude, Destination))
        {
            const FVector DashStart = Hero->GetActorLocation();
            const bool bMoved = Hero->SetActorLocation(
                Destination, true, &MoveHit, ETeleportType::TeleportPhysics);
            // SetActorLocation扫掠受阻后仍可能走完部分距离：已经移动就按实际短移成功，
            // 不能角色发生位移却向客户端报告“完全失败”。未动才属于失败。
            if (bMoved && FVector::DistSquared(Hero->GetActorLocation(), DashStart) >
                FMath::Square(1.0f))
            {
                Hero->ForceNetUpdate();
                bSuccess = true;
            }
        }
    }
    else if (ExecutionMode == EDivineBeastsV6NativeMode::RadialDamage ||
             ExecutionMode == EDivineBeastsV6NativeMode::RadialDamageAndStun)
    {
        // 非原子AOE：每个目标有独立真实战斗结果。一个目标在Commit回调中死亡、
        // 移出范围或抵抗控制时，跳过该目标但不抹掉已发生的其它目标伤害。
        bool bAnyApplied = false;
        for (const FV6TargetSnapshot& Target : AreaTargets)
        {
            if (!IsCurrentActivation(Token, Source, Combat, SourceGeneration))
                break; // 来源Avatar已失效：立即停止后续目标
            if (ApplyToTarget(Target))
                bAnyApplied = true;
        }
        bSuccess = bAnyApplied;
    }
    else
        bSuccess = ApplyToTarget(SingleTarget);

    if (ActivationToken == Token && IsActive())
    {
        if (bSuccess && IsCurrentActivation(Token, Source, Combat, SourceGeneration))
            if (auto* CastFacts = Source->FindComponentByClass<UGamePlatformAbilityCastComponent>())
                CastFacts->PublishConfirmedCast(Token, Balance.AbilityId,
                    Definition->DraftPresentationSemanticId, SourceGeneration, 0.35f);
        if (ActivationToken == Token && IsActive())
            EndAbility(Handle, ActorInfo, ActivationInfo, true, !bSuccess);
    }
}

void UDivineBeastsV6GameplayAbility::EndAbility(
    const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility, bool bWasCancelled)
{
    ActivationToken.Invalidate();
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

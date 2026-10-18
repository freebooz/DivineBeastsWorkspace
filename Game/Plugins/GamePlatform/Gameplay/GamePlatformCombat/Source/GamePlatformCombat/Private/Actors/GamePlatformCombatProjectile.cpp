// 平台投射物实现：必要运动Tick仅服务器开启；视觉、项目命名及施法平衡均不属于本文件。
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

bool FGamePlatformCombatProjectileMotion::IsValid() const
{
    return FMath::IsFinite(SpeedCmPerSecond) && SpeedCmPerSecond > 0 && SpeedCmPerSecond <= 20000 &&
        FMath::IsFinite(RangeCm) && RangeCm > 0 && RangeCm <= 100000 &&
        FMath::IsFinite(CollisionRadiusCm) && CollisionRadiusCm >= 1 && CollisionRadiusCm <= 100 &&
        FMath::IsFinite(HelixRadiusCm) && HelixRadiusCm >= 0 && HelixRadiusCm <= 100 &&
        FMath::IsFinite(HelixTurnsPerSecond) && HelixTurnsPerSecond >= 0 && HelixTurnsPerSecond <= 10 &&
        ((HelixRadiusCm == 0 && HelixTurnsPerSecond == 0) || (HelixRadiusCm > 0 && HelixTurnsPerSecond > 0)) &&
        FMath::IsFinite(InitialPhaseTurns) && InitialPhaseTurns >= 0 && InitialPhaseTurns < 1;
}

AGamePlatformCombatProjectile::AGamePlatformCombatProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    bReplicates = true;
    SetReplicateMovement(true);
    SetNetUpdateFrequency(60.0f);
    CollisionRoot = CreateDefaultSubobject<USphereComponent>(TEXT("ProjectileRoot"));
    SetRootComponent(CollisionRoot);
    CollisionRoot->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

bool AGamePlatformCombatProjectile::HasCurrentSource() const
{
    const AActor* Source = CastSpec.Source.Get();
    const auto* Combat = IsValid(Source) ? Source->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    return HasAuthority() && IsValid(Source) && Source->HasAuthority() && Source->GetWorld() == GetWorld() &&
        Combat && !Combat->IsCombatDead() && Combat->GetCombatAvatarGeneration() == CastSpec.SourceAvatarGeneration;
}

bool AGamePlatformCombatProjectile::AuthorityInitialize(const FGamePlatformCombatSpec& Spec,
    const FGamePlatformCombatProjectileMotion& Motion, const FVector& Direction, FName StyleId)
{
    check(IsInGameThread());
    if (!HasAuthority() || Facts.bStarted || Facts.bFinished || !GetWorld() || GetWorld()->bIsTearingDown ||
        !Spec.EventId.IsValid() || !FMath::IsFinite(Spec.Magnitude) || Spec.Magnitude <= 0 ||
        Spec.SourceAvatarGeneration <= 0 || !Motion.IsValid() || Direction.ContainsNaN() || Direction.IsNearlyZero()) return false;
    CastSpec = Spec;
    if (!HasCurrentSource()) { CastSpec = {}; return false; }
    FlightMotion = Motion;
    Origin = GetActorLocation(); Axis = Direction.GetSafeNormal();
    bHasHomingTarget = Spec.Target != nullptr;
    HomingTarget = Spec.Target.Get(); HomingTargetAvatarGeneration = Spec.TargetAvatarGeneration;
    HomingCenter = Origin;
    if (!HasCurrentHomingTarget() || (bHasHomingTarget &&
        FVector::DistSquared(Origin,HomingTarget->FindComponentByClass<UGamePlatformCombatComponent>()->GetCombatTargetPoint()) > FMath::Square(Motion.RangeCm)))
    {
        CastSpec={}; HomingTarget.Reset(); bHasHomingTarget=false; return false;
    }
    // 弹体不持有目标强引用；仅冻结其身份，真正命中仍取本次服务器Sweep结果。
    CastSpec.Target=nullptr;
    // 将世界Up投影为稳定截面；竖直射击改用Right，避免退化基底导致螺旋塌为直线。
    RadialY = FVector::CrossProduct(FVector::UpVector, Axis).GetSafeNormal();
    if (RadialY.IsNearlyZero()) RadialY = FVector::RightVector;
    RadialZ = FVector::CrossProduct(Axis, RadialY).GetSafeNormal();
    FlightSeconds = 0;
    Facts.OccurrenceId = Spec.EventId; Facts.AbilityId = Spec.SourceAbilityId;
    Facts.SourceAvatarGeneration = Spec.SourceAvatarGeneration; Facts.PresentationStyleId = StyleId;
    Facts.bStarted = true;
    SetOwner(Spec.Source.Get()); SetActorTickEnabled(true);
    ForceNetUpdate(); OnRep_Facts();
    return true;
}

bool AGamePlatformCombatProjectile::HasCurrentHomingTarget() const
{
    if (!bHasHomingTarget) return true;
    const AActor* Target=HomingTarget.Get();
    const auto* Combat=IsValid(Target) ? Target->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
    return IsValid(Target) && !Target->IsActorBeingDestroyed() && Target!=CastSpec.Source.Get() &&
        Target->HasAuthority() && Target->GetWorld()==GetWorld() && !Target->GetActorLocation().ContainsNaN() &&
        Combat && !Combat->IsCombatDead() && HomingTargetAvatarGeneration>0 &&
        Combat->GetCombatAvatarGeneration()==HomingTargetAvatarGeneration;
}

FVector AGamePlatformCombatProjectile::AdvanceHomingPosition(float StepSeconds)
{
    const FVector TargetCenter = HomingTarget->FindComponentByClass<UGamePlatformCombatComponent>()->GetCombatTargetPoint();
    const FVector Desired=(TargetCenter-HomingCenter).GetSafeNormal();
    if (!Desired.IsNearlyZero()) Axis=Desired;
    // 传递正交截面；换轴后先补偿当前相位偏移，保证本段从真实当前位置连续起步。
    RadialY=(RadialY-Axis*FVector::DotProduct(RadialY,Axis)).GetSafeNormal();
    if (RadialY.IsNearlyZero()) RadialY=FVector::CrossProduct(FVector::UpVector,Axis).GetSafeNormal(UE_SMALL_NUMBER,FVector::RightVector);
    RadialZ=FVector::CrossProduct(Axis,RadialY).GetSafeNormal();
    const double Initial=FlightMotion.InitialPhaseTurns*UE_DOUBLE_TWO_PI;
    const double PreviousPhase=Initial+FlightSeconds*FlightMotion.HelixTurnsPerSecond*UE_DOUBLE_TWO_PI;
    // 不能用新截面重新旋转全部历史位移；只推进相邻相位差，避免急转时出现跨障碍的扫掠跳段。
    HomingCenter=GetActorLocation()-FlightMotion.HelixRadiusCm*(
        RadialY*(FMath::Cos(PreviousPhase)-FMath::Cos(Initial))+
        RadialZ*(FMath::Sin(PreviousPhase)-FMath::Sin(Initial)));
    HomingCenter+=Axis*(StepSeconds*FlightMotion.SpeedCmPerSecond);
    const double Phase=Initial+(FlightSeconds+StepSeconds)*FlightMotion.HelixTurnsPerSecond*UE_DOUBLE_TWO_PI;
    return HomingCenter+FlightMotion.HelixRadiusCm*(RadialY*(FMath::Cos(Phase)-FMath::Cos(Initial))+
        RadialZ*(FMath::Sin(Phase)-FMath::Sin(Initial)));
}

FVector AGamePlatformCombatProjectile::PositionAt(float Seconds) const
{
    const double Initial = FlightMotion.InitialPhaseTurns * UE_DOUBLE_TWO_PI;
    const double Phase = Initial + Seconds * FlightMotion.HelixTurnsPerSecond * UE_DOUBLE_TWO_PI;
    // 减去初相位偏移使真实出生位置连续；轴线与出生位置相距Radius，弹体实际围绕轴旋转。
    return Origin + Axis * (Seconds * FlightMotion.SpeedCmPerSecond) + FlightMotion.HelixRadiusCm *
        (RadialY * (FMath::Cos(Phase) - FMath::Cos(Initial)) + RadialZ * (FMath::Sin(Phase) - FMath::Sin(Initial)));
}

void AGamePlatformCombatProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || !Facts.bStarted || Facts.bFinished) return;
    if (!HasCurrentSource() || !GetWorld() || GetWorld()->bIsTearingDown) { FinishFlight(nullptr); return; }
    if (!FMath::IsFinite(DeltaSeconds) || DeltaSeconds <= 0) return;
    // 服务器卡顿最多推进0.25秒且每次最多256段；不直接跨越整段曲线/障碍，超额时间只使飞行变慢。
    // 每段不超过1/120秒、15厘米轴向或1/32圈，防止高速弹和螺旋在帧间穿透。
    float Remaining = FMath::Min(DeltaSeconds, 0.25f);
    const float StepLimit = FMath::Min(1.0f / 120.0f, FMath::Min(15.0f / FlightMotion.SpeedCmPerSecond,
        FlightMotion.HelixTurnsPerSecond > 0 ? 1.0f / (32 * FlightMotion.HelixTurnsPerSecond) : 1.0f));
    const float EndSeconds = FlightMotion.RangeCm / FlightMotion.SpeedCmPerSecond;
    FCollisionQueryParams Query(TEXT("GamePlatformCombatProjectile"), false, this);
    Query.AddIgnoredActor(CastSpec.Source.Get());
    for (int32 Segment = 0; Segment < 256 && Remaining > UE_SMALL_NUMBER && !Facts.bFinished; ++Segment)
    {
        if (!HasCurrentHomingTarget()) { FinishFlight(nullptr); break; }
        const float Step = FMath::Min(Remaining, FMath::Min(StepLimit, EndSeconds - FlightSeconds));
        if (Step <= UE_SMALL_NUMBER) { FinishFlight(nullptr); break; }
        const FVector Start = GetActorLocation();
        const FVector End = bHasHomingTarget ? AdvanceHomingPosition(Step) : PositionAt(FlightSeconds + Step);
        FHitResult VisibilityHit;
        FHitResult PawnHit;
        const FCollisionShape Shape = FCollisionShape::MakeSphere(FlightMotion.CollisionRadiusCm);
        const bool bVisibilityHit = GetWorld()->SweepSingleByChannel(
            VisibilityHit, Start, End, FQuat::Identity, ECC_Visibility, Shape, Query);
        // Pawn胶囊可合法忽略Visibility；另查Pawn对象，并只接受比世界遮挡更近的命中。
        const bool bPawnHit = GetWorld()->SweepSingleByObjectType(
            PawnHit, Start, End, FQuat::Identity, FCollisionObjectQueryParams(ECC_Pawn), Shape, Query);
        const FHitResult* FirstHit = bPawnHit && (!bVisibilityHit || PawnHit.Distance < VisibilityHit.Distance)
            ? &PawnHit : (bVisibilityHit ? &VisibilityHit : nullptr);
        if (FirstHit)
        {
            SetActorLocation(FirstHit->Location, false); FinishFlight(FirstHit); break;
        }
        FlightSeconds += Step; Remaining -= Step;
        if (bHasHomingTarget) SetActorLocationAndRotation(End,(End-Start).Rotation(),false);
        else SetActorLocation(End, false);
        if (FlightSeconds >= EndSeconds - UE_SMALL_NUMBER) FinishFlight(nullptr);
    }
}

void AGamePlatformCombatProjectile::FinishFlight(const FHitResult* Hit)
{
    if (!HasAuthority() || Facts.bFinished) return;
    Facts.bFinished = true; SetActorTickEnabled(false);
    Facts.bImpacted = Hit && Hit->bBlockingHit;
    if (Hit)
    {
        Facts.ImpactLocation = Hit->ImpactPoint; Facts.ImpactNormal = Hit->ImpactNormal.GetSafeNormal();
        AActor* Target = Hit->GetActor();
        auto* SourceCombat = IsValid(CastSpec.Source.Get()) ? CastSpec.Source->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
        auto* TargetCombat = IsValid(Target) ? Target->FindComponentByClass<UGamePlatformCombatComponent>() : nullptr;
        // 物理Sweep仍决定是否被遮挡及命中哪个实体；复制的表现点统一放在实际受击实体中心。
        // 不把穿入胶囊中心的视觉定位冒充新的碰撞证据；Damage.HitContext保留真实表面命中。
        if (TargetCombat && !TargetCombat->IsCombatDead())
        {
            Facts.ImpactLocation = TargetCombat->GetCombatTargetPoint();
            SetActorLocation(Facts.ImpactLocation, false);
        }
        if (HasCurrentSource() && HasCurrentHomingTarget() && SourceCombat && TargetCombat && Target != CastSpec.Source.Get())
        {
            FGamePlatformCombatSpec Damage = CastSpec;
            Damage.Target = Target; Damage.TargetAvatarGeneration = TargetCombat->GetCombatAvatarGeneration();
            Damage.HitContext.bHasValidatedHit = true; Damage.HitContext.TraceStart = Origin;
            Damage.HitContext.TraceEnd = Hit->ImpactPoint; Damage.HitContext.ImpactPoint = Hit->ImpactPoint;
            Damage.HitContext.ImpactNormal = Hit->ImpactNormal; Damage.HitContext.ValidatedDistance = FVector::Distance(Origin, Hit->ImpactPoint);
            SourceCombat->ApplyDamage(Damage); // 唯一防御/护盾/生命/GAS入口；拒绝也不冒充成功伤害。
        }
    }
    CastSpec = {};
    HomingTarget.Reset(); bHasHomingTarget=false; HomingTargetAvatarGeneration=0;
    if (!IsActorBeingDestroyed()) { ForceNetUpdate(); OnRep_Facts(); SetLifeSpan(0.25f); }
}
void AGamePlatformCombatProjectile::AuthorityCancel() { if (HasAuthority()) FinishFlight(nullptr); }
void AGamePlatformCombatProjectile::OnRep_Facts() { FactsChanged.Broadcast(this, Facts); }
void AGamePlatformCombatProjectile::BeginPlay() { Super::BeginPlay(); OnRep_Facts(); }
void AGamePlatformCombatProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
    Facts.bFinished = true; SetActorTickEnabled(false); CastSpec = {}; HomingTarget.Reset(); bHasHomingTarget=false;
    FactsChanged.Broadcast(this, Facts); FactsChanged.Clear(); Super::EndPlay(Reason);
}
void AGamePlatformCombatProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    // DOREPLIFETIME宏按引擎合同引用OutLifetimeProps，参数名不是可随意替换的局部简写。
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AGamePlatformCombatProjectile, Facts);
}

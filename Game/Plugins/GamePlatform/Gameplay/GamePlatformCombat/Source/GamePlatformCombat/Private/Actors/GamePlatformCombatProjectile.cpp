// 平台投射物实现：必要运动Tick仅服务器开启；视觉、项目命名及施法平衡均不属于本文件。
#include "Actors/GamePlatformCombatProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/GamePlatformCombatComponent.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"

bool FGamePlatformCombatProjectileMotion::IsValid() const
{
    return FMath::IsFinite(SpeedCmPerSecond) && SpeedCmPerSecond > 0 && SpeedCmPerSecond <= 20000 &&
        FMath::IsFinite(RangeCm) && RangeCm > 0 && RangeCm <= 100000 &&
        FMath::IsFinite(CollisionRadiusCm) && CollisionRadiusCm >= 1 && CollisionRadiusCm <= 100 &&
        FMath::IsFinite(HelixRadiusCm) && HelixRadiusCm >= 0 && HelixRadiusCm <= 100 &&
        FMath::IsFinite(HelixTurnsPerSecond) && HelixTurnsPerSecond >= 0 && HelixTurnsPerSecond <= 10 &&
        ((HelixRadiusCm == 0 && HelixTurnsPerSecond == 0) || (HelixRadiusCm > 0 && HelixTurnsPerSecond > 0)) &&
        FMath::IsFinite(InitialPhaseTurns) && InitialPhaseTurns >= 0 && InitialPhaseTurns < 1 &&
        (!ResolveFlightConfig().bEnabled || ResolveFlightConfig().IsValid());
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
    FlightInitialization.OccurrenceId=Spec.EventId;
    FlightInitialization.Config=bHasPendingFlight ? PendingFlight : Motion.ResolveFlightConfig();
    FlightInitialization.LaunchDirection=Direction.GetSafeNormal();
    FlightInitialization.bHasSecondaryVelocity=Motion.HelixRadiusCm>0 || FlightInitialization.Config.GravityScale!=0;
    FlightInitialization.ServerStartTime=GetWorld()->GetTimeSeconds();
    OnRep_FlightInitialization();
    Origin = GetActorLocation(); Axis = Direction.GetSafeNormal();
    bHasHomingTarget = Spec.Target != nullptr && (!FrozenFlight.bEnabled || FrozenFlight.bEnableHoming);
    bDirectTargetHoming = false;
    HomingTarget = Spec.Target.Get(); HomingTargetAvatarGeneration = Spec.TargetAvatarGeneration;
    HomingCenter = Origin;
    HomingHelixRadiusCm = Motion.HelixRadiusCm;
    const float LaunchSpeed=FrozenFlight.bEnabled ? FrozenFlight.InitialSpeed : Motion.SpeedCmPerSecond;
    FlightEndSeconds = Motion.RangeCm / FMath::Max(LaunchSpeed,1.f);
    FlightDistanceLimitCm=Motion.RangeCm;
    if (!HasCurrentHomingTarget())
    {
        CastSpec={}; HomingTarget.Reset(); bHasHomingTarget=false; return false;
    }
    if (bHasHomingTarget)
    {
        const FVector TargetCenter = HomingTarget->FindComponentByClass<UGamePlatformCombatComponent>()->GetCombatTargetPoint();
        const FVector SourceCenter = Spec.Source->FindComponentByClass<UGamePlatformCombatComponent>()->GetCombatTargetPoint();
        // 锁定距离从施法者中心校验，不能因向前的手部起射偏移而拒绝后方的合法射程边界目标。
        // 任意方向的浮点中心可能比精确射程多出末位误差；0.01厘米容差只吸收数值误差。
        if (FVector::DistSquared(SourceCenter, TargetCenter) > FMath::Square(static_cast<double>(Motion.RangeCm) + .01))
        {
            CastSpec={}; HomingTarget.Reset(); bHasHomingTarget=false; return false;
        }
        // 只在发射时按角色中心分类，手部起射偏移不能改变正向锥资格。
        // 后续仍追踪冻结的原目标；小于30度只取消额外Bezier弧线，不锁死旧坐标。
        const FVector SourceToTarget = (TargetCenter - SourceCenter).GetSafeNormal(UE_SMALL_NUMBER, Axis);
        bDirectTargetHoming = FVector::DotProduct(Axis, SourceToTarget) >
            FMath::Cos(FMath::DegreesToRadians(30.0)) + UE_DOUBLE_SMALL_NUMBER;
        BuildHomingBezier(TargetCenter);
        // 射程是中心间施放资格，不是弧线路程。初次弧长加有界捕获余量冻结期限，后续重建不能延寿。
        FlightEndSeconds = FMath::Max(FlightEndSeconds, BezierArcLengths[32] / FMath::Max(LaunchSpeed,1.f)) +
            FMath::Min(.5f, .5f * FlightEndSeconds);
        FlightDistanceLimitCm=FMath::Max(Motion.RangeCm,BezierArcLengths[32])+LaunchSpeed*.5f;
    }
    // 弹体不持有目标强引用；仅冻结其身份，真正命中仍取本次服务器Sweep结果。
    CastSpec.Target=nullptr;
    // 将世界Up投影为稳定截面；竖直射击改用Right，避免退化基底导致螺旋塌为直线。
    RadialY = FVector::CrossProduct(FVector::UpVector, Axis).GetSafeNormal();
    if (RadialY.IsNearlyZero()) RadialY = FVector::RightVector;
    RadialZ = FVector::CrossProduct(Axis, RadialY).GetSafeNormal();
    FlightSeconds = 0;
    TravelledDistanceCm=0;
    if(FrozenFlight.bEnabled)
    {
        // 新策略以显式寿命/真实累计距离收敛；不能用旧Range/初速提前截断减速或悬停。
        FlightEndSeconds=FrozenFlight.MaxFlightTime;
        FlightDistanceLimitCm=FMath::Min(FlightDistanceLimitCm,FrozenFlight.MaxFlightDistance);
    }
    CurrentVelocity=FrozenFlight.bEnabled ? BallisticVelocityAt(0) : Axis*LaunchSpeed;
    Facts.OccurrenceId = Spec.EventId; Facts.AbilityId = Spec.SourceAbilityId;
    Facts.SourceAvatarGeneration = Spec.SourceAvatarGeneration; Facts.PresentationStyleId = StyleId;
    Facts.bStarted = true;
    if(!FrozenFlight.bEnabled || FrozenFlight.bRotationFollowsVelocity)
        SetActorRotation(CurrentVelocity.GetSafeNormal(UE_SMALL_NUMBER,Axis).Rotation());
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

void AGamePlatformCombatProjectile::BuildHomingBezier(const FVector& TargetCenter)
{
    BezierStart = HomingCenter; BezierEnd = TargetCenter; BezierDistanceCm = 0.f;
    const FVector Delta = TargetCenter - HomingCenter;
    const float Distance = Delta.Size();
    const FVector Desired = Delta.GetSafeNormal(UE_SMALL_NUMBER, Axis);
    if (bDirectTargetHoming)
    {
        BezierControl1 = BezierStart + Delta / 3.0;
        BezierControl2 = BezierStart + Delta * (2.0 / 3.0);
        // 直追允许朝向移动目标立即调整；否则固定的单段转角门禁会反复缩步至停飞。
        // 真实段长、Sweep、有限期限及显式螺旋参数继续走同一运动链。
        Axis = Desired;
    }
    else
    {
        const float Angle = FMath::Acos(FMath::Clamp(FVector::DotProduct(Axis, Desired), -1.0, 1.0));
        FVector Normal = FVector::CrossProduct(Axis, Desired).GetSafeNormal();
        if (Normal.IsNearlyZero())
        {
            // 正后方也必须有非共线控制点，不能在直线上折返。
            Normal = (FVector::UpVector - Axis * FVector::DotProduct(FVector::UpVector, Axis)).GetSafeNormal();
            if (Normal.IsNearlyZero()) Normal = FVector::RightVector;
        }
        const FVector Side = FVector::CrossProduct(Desired, Normal).GetSafeNormal();
        const float Handle = Distance * FMath::Lerp(.35f, .60f, Angle / UE_PI);
        BezierControl1 = BezierStart + Axis * Handle;
        BezierControl2 = BezierEnd - Desired * Handle + Side * (Handle * FMath::Sin(.5f * Angle));
    }
    FVector Previous = BezierStart;
    BezierArcLengths[0] = 0.f;
    for (int32 Index = 1; Index <= 32; ++Index)
    {
        const float T = Index / 32.f, U = 1.f - T;
        const FVector Point = U*U*U*BezierStart + 3.f*U*U*T*BezierControl1 + 3.f*U*T*T*BezierControl2 + T*T*T*BezierEnd;
        BezierArcLengths[Index] = BezierArcLengths[Index-1] + FVector::Distance(Previous, Point);
        Previous = Point;
    }
}

FVector AGamePlatformCombatProjectile::EvaluateHomingBezier(float DistanceCm, FVector& OutTangent) const
{
    int32 Index = 1;
    while (Index < 32 && BezierArcLengths[Index] < DistanceCm) ++Index;
    const float SegmentLength = BezierArcLengths[Index] - BezierArcLengths[Index-1];
    const float Fraction = SegmentLength > UE_SMALL_NUMBER ? FMath::Clamp((DistanceCm-BezierArcLengths[Index-1])/SegmentLength, 0.f, 1.f) : 1.f;
    const float T = (Index-1+Fraction)/32.f, U = 1.f-T;
    OutTangent = (3.f*U*U*(BezierControl1-BezierStart) + 6.f*U*T*(BezierControl2-BezierControl1) +
        3.f*T*T*(BezierEnd-BezierControl2)).GetSafeNormal(UE_SMALL_NUMBER, Axis);
    return U*U*U*BezierStart + 3.f*U*U*T*BezierControl1 + 3.f*U*T*T*BezierControl2 + T*T*T*BezierEnd;
}

FVector AGamePlatformCombatProjectile::AdvanceHomingPosition(float& StepSeconds, const FVector& TargetCenter)
{
    // 每个子步读取当前目标；只有目标实际移动才从当前位置/切线重建，静止目标完整沿一条三次曲线飞行。
    if (!TargetCenter.Equals(BezierEnd, .001)) BuildHomingBezier(TargetCenter);
    FVector NextCenter, NextAxis, NextY, NextZ, End;
    float NextDistance = 0.f, NextRadius = HomingHelixRadiusCm;
    for (int32 Attempt = 0; Attempt < 20; ++Attempt)
    {
        NextDistance = FMath::Min(BezierDistanceCm + PropulsionDistance(FlightSeconds,FlightSeconds+StepSeconds), BezierArcLengths[32]);
        if(FrozenFlight.bEnabled && FrozenFlight.HomingAcceleration>0 && !bDirectTargetHoming)
        {
            const FVector Desired=(TargetCenter-HomingCenter).GetSafeNormal(UE_SMALL_NUMBER,Axis);
            const float Degrees=FMath::RadiansToDegrees(FrozenFlight.HomingAcceleration/FMath::Max(1.f,FrozenFlight.SpeedAt(FlightSeconds)));
            NextAxis=FMath::VInterpNormalRotationTo(Axis,Desired,StepSeconds,Degrees);
            const float Distance=PropulsionDistance(FlightSeconds,FlightSeconds+StepSeconds);
            NextCenter=HomingCenter+NextAxis*Distance;
        }
        else NextCenter = EvaluateHomingBezier(NextDistance, NextAxis);
        NextY = (RadialY - NextAxis * FVector::DotProduct(RadialY, NextAxis)).GetSafeNormal();
        if (NextY.IsNearlyZero()) NextY = FVector::CrossProduct(FVector::UpVector, NextAxis).GetSafeNormal(UE_SMALL_NUMBER, FVector::RightVector);
        NextZ = FVector::CrossProduct(NextAxis, NextY).GetSafeNormal();
        if (HomingHelixRadiusCm > 0.f)
        {
            const float DesiredRadius = FlightMotion.HelixRadiusCm * FMath::Clamp(
                static_cast<float>(FVector::Distance(GetActorLocation(), TargetCenter))/(6.f*FlightMotion.HelixRadiusCm), 0.f, 1.f);
            NextRadius = FMath::FInterpConstantTo(HomingHelixRadiusCm, FMath::Min(HomingHelixRadiusCm, DesiredRadius), StepSeconds,
                .5f*(FrozenFlight.bEnabled ? FrozenFlight.SpeedAt(FlightSeconds) : FlightMotion.SpeedCmPerSecond));
        }
        const double Initial = FlightMotion.InitialPhaseTurns * UE_DOUBLE_TWO_PI;
        const double PreviousPhase = Initial + FlightSeconds*FlightMotion.HelixTurnsPerSecond*UE_DOUBLE_TWO_PI;
        const double Phase = Initial + (FlightSeconds+StepSeconds)*FlightMotion.HelixTurnsPerSecond*UE_DOUBLE_TWO_PI;
        // 已飞过的螺旋偏移保持世界位置，只在新截面累加本段相位差，不能将累计相位重新旋转造成横跳。
        // 接近目标时将真实累计偏移同比收束到零，避免中心线抵达而螺旋弹体始终绕过目标。
        const FVector PreviousOffset = HomingHelixRadiusCm > UE_SMALL_NUMBER ?
            (GetActorLocation()-HomingCenter)*(NextRadius/HomingHelixRadiusCm) : FVector::ZeroVector;
        End = NextCenter + PreviousOffset + NextRadius * (NextY*(FMath::Cos(Phase)-FMath::Cos(PreviousPhase)) +
            NextZ*(FMath::Sin(Phase)-FMath::Sin(PreviousPhase)));
        // 弧长表离散误差、曲率和螺旋换轴都按真实段长再约束，不能以曲线中心的步长掩盖跨障碍跳段。
        if (FVector::DistSquared(GetActorLocation(), End) <= FMath::Square(15.01) &&
            FVector::DotProduct(Axis, NextAxis) >= FMath::Cos(UE_PI/16.f)) break;
        if (Attempt == 19) { StepSeconds = 0.f; return GetActorLocation(); }
        StepSeconds *= .5f;
    }
    HomingCenter = NextCenter; BezierDistanceCm = NextDistance; Axis = NextAxis;
    RadialY = NextY; RadialZ = NextZ; HomingHelixRadiusCm = NextRadius;
    return End;
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
    const float SpeedBound=FrozenFlight.bEnabled ? FrozenFlight.MaxSpeed : FlightMotion.SpeedCmPerSecond;
    const bool bSimpleUnsubstepped=FrozenFlight.bEnabled && !FrozenFlight.bEnableSubStepping &&
        !bHasHomingTarget && FrozenFlight.GravityScale==0 && FlightMotion.HelixRadiusCm==0;
    const float StepLimit = bSimpleUnsubstepped ? .25f : FMath::Min(1.0f / 120.0f, FMath::Min(15.0f / SpeedBound,
        FlightMotion.HelixTurnsPerSecond > 0 ? 1.0f / (32 * FlightMotion.HelixTurnsPerSecond) : 1.0f));
    FCollisionQueryParams Query(TEXT("GamePlatformCombatProjectile"), false, this);
    Query.AddIgnoredActor(CastSpec.Source.Get());
    for (int32 Segment = 0; Segment < 256 && Remaining > UE_SMALL_NUMBER && !Facts.bFinished; ++Segment)
    {
        if (!HasCurrentHomingTarget()) { FinishFlight(nullptr); break; }
        FVector TargetCenter = FVector::ZeroVector;
        float Step = FMath::Min(Remaining, FMath::Min(StepLimit, FlightEndSeconds - FlightSeconds));
        if (bHasHomingTarget)
        {
            TargetCenter = HomingTarget->FindComponentByClass<UGamePlatformCombatComponent>()->GetCombatTargetPoint();
        }
        if (Step <= UE_SMALL_NUMBER) { FinishFlight(nullptr); break; }
        const FVector Start = GetActorLocation();
        FVector End = bHasHomingTarget ? AdvanceHomingPosition(Step, TargetCenter) : AdvanceFreePosition(Step);
        if (Step <= UE_SMALL_NUMBER) { FinishFlight(nullptr); break; } // 数值退化拒绝跳段，不伪造命中。
        if(FrozenFlight.bEnabled)
        {
            End=Start+(End-Start).GetClampedToMaxSize(SpeedBound*Step);
            const double Distance=(End-Start).Size();
            const double Left=FMath::Max(0.f,FlightDistanceLimitCm-TravelledDistanceCm);
            if(Distance>Left && Distance>UE_SMALL_NUMBER)End=Start+(End-Start)*(Left/Distance);
        }
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
        TravelledDistanceCm+=FVector::Distance(Start,End);
        CurrentVelocity=FrozenFlight.bEnabled && !bHasHomingTarget ? BallisticVelocityAt(FlightSeconds) :
            (FrozenFlight.bEnabled && FlightMotion.HelixRadiusCm>0 ? (End-Start)/Step :
            (End-Start).GetSafeNormal()* (FrozenFlight.bEnabled ? FrozenFlight.SpeedAt(FlightSeconds) : FlightMotion.SpeedCmPerSecond));
        CollisionRoot->ComponentVelocity=CurrentVelocity;
        if ((!FrozenFlight.bEnabled && bHasHomingTarget) || (FrozenFlight.bEnabled && FrozenFlight.bRotationFollowsVelocity))
            SetActorLocationAndRotation(End,CurrentVelocity.IsNearlyZero() ? GetActorRotation() : CurrentVelocity.Rotation(),false);
        else SetActorLocation(End, false);
        DispatchFlightEvents();
        if (FlightSeconds >= FlightEndSeconds - UE_SMALL_NUMBER ||
            (FrozenFlight.bEnabled && TravelledDistanceCm>=FlightDistanceLimitCm-UE_SMALL_NUMBER)) FinishFlight(nullptr);
    }
}

void AGamePlatformCombatProjectile::FinishFlight(const FHitResult* Hit)
{
    if (!HasAuthority() || Facts.bFinished) return;
    Facts.bFinished = true; SetActorTickEnabled(false); CurrentVelocity=FVector::ZeroVector;
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
void AGamePlatformCombatProjectile::OnRep_Facts() { DispatchFlightEvents(); FactsChanged.Broadcast(this, Facts); }
void AGamePlatformCombatProjectile::BeginPlay() { Super::BeginPlay(); OnRep_Facts(); }
void AGamePlatformCombatProjectile::EndPlay(const EEndPlayReason::Type Reason)
{
    Facts.bFinished = true; CurrentVelocity=FVector::ZeroVector;SetActorTickEnabled(false); CastSpec = {}; HomingTarget.Reset(); bHasHomingTarget=false;
    DispatchFlightEvents();
    FactsChanged.Broadcast(this, Facts); FactsChanged.Clear(); Super::EndPlay(Reason);
}
void AGamePlatformCombatProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    // DOREPLIFETIME宏按引擎合同引用OutLifetimeProps，参数名不是可随意替换的局部简写。
    Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(AGamePlatformCombatProjectile, Facts);
    DOREPLIFETIME(AGamePlatformCombatProjectile, FlightInitialization);
}

bool AGamePlatformCombatProjectile::AuthoritySetFlightConfig(const FGamePlatformCombatProjectileFlightConfig& Config)
{
    if(!HasAuthority() || Facts.bStarted || Facts.bFinished || !Config.IsValid())return false;
    PendingFlight=Config;bHasPendingFlight=true;return true;
}

void AGamePlatformCombatProjectile::OnRep_FlightInitialization()
{
    FrozenFlight=FlightInitialization.Config;
    if(!HasAuthority())Axis=FlightInitialization.LaunchDirection;
    // 冻结曲线关键帧，后续共享资产/模板编辑不能改变已发射弹体。
    if(FrozenFlight.SpeedCurve)
    {
        UCurveFloat* Copy=NewObject<UCurveFloat>(this);Copy->FloatCurve=FrozenFlight.SpeedCurve->FloatCurve;
        FrozenFlight.SpeedCurve=Copy;
    }
    DispatchFlightEvents();
}

float AGamePlatformCombatProjectile::PropulsionDistance(float A,float B) const
{ return FrozenFlight.bEnabled ? FrozenFlight.DistanceBetween(A,B) : FlightMotion.SpeedCmPerSecond*(B-A); }

FVector AGamePlatformCombatProjectile::BallisticVelocityAt(float Seconds) const
{
    const FVector Gravity(0,0,GetWorld()->GetGravityZ()*FrozenFlight.GravityScale*Seconds);
    const double Omega=FlightMotion.HelixTurnsPerSecond*UE_DOUBLE_TWO_PI;
    const double Phase=FlightMotion.InitialPhaseTurns*UE_DOUBLE_TWO_PI+Seconds*Omega;
    const FVector Helix=FlightMotion.HelixRadiusCm*Omega*(-RadialY*FMath::Sin(Phase)+RadialZ*FMath::Cos(Phase));
    return (Axis*FrozenFlight.SpeedAt(Seconds)+Gravity+Helix).GetClampedToMaxSize(FrozenFlight.MaxSpeed);
}

FVector AGamePlatformCombatProjectile::AdvanceFreePosition(float Step) const
{
    FVector Delta;
    if(!FrozenFlight.bEnabled || (FrozenFlight.GravityScale==0 && FlightMotion.HelixRadiusCm==0))
        Delta=Axis*PropulsionDistance(FlightSeconds,FlightSeconds+Step);
    else Delta=Step*(BallisticVelocityAt(FlightSeconds)+4*BallisticVelocityAt(FlightSeconds+Step*.5f)+BallisticVelocityAt(FlightSeconds+Step))/6;
    const double P=FlightMotion.InitialPhaseTurns*UE_DOUBLE_TWO_PI;
    const double A=P+FlightSeconds*FlightMotion.HelixTurnsPerSecond*UE_DOUBLE_TWO_PI;
    const double B=P+(FlightSeconds+Step)*FlightMotion.HelixTurnsPerSecond*UE_DOUBLE_TWO_PI;
    if(!FrozenFlight.bEnabled)
        Delta+=FlightMotion.HelixRadiusCm*(RadialY*(FMath::Cos(B)-FMath::Cos(A))+RadialZ*(FMath::Sin(B)-FMath::Sin(A)));
    return GetActorLocation()+Delta;
}

FVector AGamePlatformCombatProjectile::GetVelocity() const
{ return Facts.bFinished ? FVector::ZeroVector : (HasAuthority() ? CurrentVelocity : GetReplicatedMovement().LinearVelocity); }

FGamePlatformMotionSnapshot AGamePlatformCombatProjectile::GetMotionSnapshot_Implementation() const
{
    FGamePlatformMotionSnapshot S;
    if(!Facts.bStarted || FlightInitialization.OccurrenceId!=Facts.OccurrenceId)return S;
    S.bValid=true;S.bEnableSpeedBasedVFX=FrozenFlight.bEnabled && FrozenFlight.bEnableSpeedBasedVFX;
    S.ElapsedSeconds=FlightSeconds;
    if(!HasAuthority())
    {
        const auto* GS=GetWorld()->GetGameState();
        if(!GS)return FGamePlatformMotionSnapshot();
        S.ElapsedSeconds=FMath::Max(0.,GS->GetServerWorldTimeSeconds()-FlightInitialization.ServerStartTime);
    }
    S.MaxSpeed=FrozenFlight.bEnabled ? FrozenFlight.MaxSpeed : FlightMotion.SpeedCmPerSecond;
    S.Speed=Facts.bFinished ? 0 : (FrozenFlight.bEnabled ? FrozenFlight.SpeedAt(S.ElapsedSeconds) : S.MaxSpeed);
    if(!HasAuthority() && FrozenFlight.bEnabled && FlightInitialization.bHasSecondaryVelocity && !Facts.bFinished)
        S.Speed=GetVelocity().Size();
    if(HasAuthority() && !Facts.bFinished)S.Speed=CurrentVelocity.Size();
    S.FlightDirection=GetVelocity().GetSafeNormal(UE_SMALL_NUMBER,GetActorForwardVector());
    if(FrozenFlight.bEnabled && !Facts.bFinished)
        S.Acceleration=(FrozenFlight.SpeedAt(S.ElapsedSeconds+.001f)-FrozenFlight.SpeedAt(S.ElapsedSeconds))/.001f;
    S.SpeedNormalized=FMath::Clamp(S.Speed/FMath::Max(S.MaxSpeed,1.f),0.f,1.f);
    S.AccelerationNormalized=FrozenFlight.bEnabled ? FMath::Clamp(S.Acceleration/FMath::Max(1.f,FMath::Max(FrozenFlight.Acceleration,FrozenFlight.Deceleration)),-1.f,1.f) : 0;
    S.FlightPhase=Facts.bFinished ? 0 : (S.Acceleration>1 ? 1 : (S.Acceleration < -1 ? 3 : 2));
    S.TrailLengthScale=FMath::Lerp(.55f,1.f,S.SpeedNormalized);return S;
}

void AGamePlatformCombatProjectile::DispatchFlightEvents()
{
    if(!Facts.bStarted || Facts.OccurrenceId!=FlightInitialization.OccurrenceId)return;
    if(Facts.bFinished)
    {
        if(!bStopEventSent){bStopEventSent=true;OnFlightStopped.Broadcast();if(Facts.bImpacted)OnProjectileImpact.Broadcast();}
        return;
    }
    if(!bLaunchEventSent){bLaunchEventSent=true;OnFlightLaunched.Broadcast();if(Facts.bFinished)return;}
    const auto S=GetMotionSnapshot_Implementation();if(!S.bValid)return;
    const int32 Previous=LastFlightPhase;LastFlightPhase=S.FlightPhase;
    if(S.FlightPhase==1 && Previous!=1)OnAccelerationStarted.Broadcast();
    if(Facts.bFinished || IsActorBeingDestroyed())return;
    if(S.FlightPhase==3 && Previous!=3)OnDecelerationStarted.Broadcast();
    if(Facts.bFinished || IsActorBeingDestroyed())return;
    if(!bMaxSpeedEventSent && FrozenFlight.bEnabled && S.Speed>=S.MaxSpeed-.1f)
    {bMaxSpeedEventSent=true;OnMaxSpeedReached.Broadcast();}
}

void AGamePlatformCombatProjectile::OnRep_ReplicatedMovement()
{ Super::OnRep_ReplicatedMovement();DispatchFlightEvents(); }

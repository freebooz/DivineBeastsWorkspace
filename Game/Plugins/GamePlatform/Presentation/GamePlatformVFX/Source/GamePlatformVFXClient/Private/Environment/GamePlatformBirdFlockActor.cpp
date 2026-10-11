// GamePlatform平台层鸟群实例化渲染：确定性轨迹+低频CPU位置更新+GPU材质振翅。
// 不引入逐鸟Actor、重物理碰撞、逐帧资源加载或任何服务器网络复制。
#include "Environment/GamePlatformBirdFlockActor.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Engine/World.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

AGamePlatformBirdFlockActor::AGamePlatformBirdFlockActor()
{
    bReplicates = false;
    SetReplicateMovement(false);
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.bStartWithTickEnabled = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("BirdFlockRoot"));
    RootComponent = Root;
    BirdInstances = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BirdInstances"));
    BirdInstances->SetupAttachment(Root);
    BirdInstances->SetMobility(EComponentMobility::Movable);
    BirdInstances->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BirdInstances->SetGenerateOverlapEvents(false);
    BirdInstances->SetCanEverAffectNavigation(false);
}

bool AGamePlatformBirdFlockActor::CanRenderLocally() const
{
    const UWorld* World = GetWorld();
    return World && World->GetNetMode() != NM_DedicatedServer && !IsActorBeingDestroyed();
}

void AGamePlatformBirdFlockActor::ClearFlock()
{
    SetActorTickEnabled(false);
    UpdateAccumulatorSeconds = 0.0f;
    Birds.Empty();
    if (BirdInstances)
    {
        BirdInstances->ClearInstances();
        BirdInstances->SetStaticMesh(nullptr);
    }
}

bool AGamePlatformBirdFlockActor::RefreshFlock()
{
    // 先清理旧批次，非法配置必须回退到“无鸟”而非留存上个物种的影子。
    ClearFlock();
    if (!CanRenderLocally() || !BirdProfile || !BirdInstances)
    {
        return false;
    }
    FString Reason;
    if (!BirdProfile->Validate(Reason))
    {
        UE_LOG(LogTemp, Warning, TEXT("鸟群配置无效：%s"), *Reason);
        return false;
    }
    const int32 Count = FGamePlatformBirdFlockMath::ResolveBirdCount(
        *BirdProfile, BirdCountOverride, QualityFraction);
    if (Count == INDEX_NONE)
    {
        UE_LOG(LogTemp, Warning, TEXT("鸟群数量或质量比例越界：Count=%d, Scale=%f"),
            BirdCountOverride, QualityFraction);
        return false;
    }
    if (Count == 0)
    {
        return true;
    }

    BirdInstances->SetStaticMesh(BirdProfile->BirdMesh);
    BirdInstances->SetMaterial(0, BirdProfile->BirdMaterial);
    BirdInstances->SetCullDistances(BirdProfile->MinDrawDistanceCm, BirdProfile->MaxDrawDistanceCm);
    BirdInstances->PreAllocateInstancesMemory(Count);
    Birds.Reserve(Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        FBirdSample Sample;
        Sample.HeadingRadians = UE_TWO_PI * FGamePlatformBirdFlockMath::SeedToUnit(FlockSeed, Index, 0x002FA6A0);
        Sample.RadiusFactor = 0.72f + 0.35f *
            FGamePlatformBirdFlockMath::SeedToUnit(FlockSeed, Index, 0x00091C21);
        Sample.HeightFactor = 2.0f *
            FGamePlatformBirdFlockMath::SeedToUnit(FlockSeed, Index, 0x005F4142) - 1.0f;
        Sample.SpeedFactor = 0.82f + 0.37f *
            FGamePlatformBirdFlockMath::SeedToUnit(FlockSeed, Index, 0x0033BA88);
        Sample.ScaleFactor = 0.78f + 0.40f *
            FGamePlatformBirdFlockMath::SeedToUnit(FlockSeed, Index, 0x00A76E11);
        Birds.Add(Sample);
        BirdInstances->AddInstance(FTransform::Identity);
    }
    UpdateBirdTransforms(SimulationTimeSeconds);
    SetActorTickEnabled(GetWorld() && GetWorld()->IsGameWorld());
    return true;
}

void AGamePlatformBirdFlockActor::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    if (bShowEditorPreview && GetWorld() && !GetWorld()->IsGameWorld())
    {
        RefreshFlock();
    }
}

void AGamePlatformBirdFlockActor::BeginPlay()
{
    Super::BeginPlay();
    SimulationTimeSeconds = 0.0;
    RefreshFlock();
}

FVector AGamePlatformBirdFlockActor::SampleLocalPosition(
    const FBirdSample& Bird, double AtSeconds) const
{
    const float Radius = FMath::Max(200.0f, BirdProfile->RadiusCm * Bird.RadiusFactor);
    const double Speed = BirdProfile->CruiseSpeedCmPerSecond * Bird.SpeedFactor;
    const double Phase = Bird.HeadingRadians;
    const double Step = AtSeconds * Speed / Radius;
    FVector Position;
    switch (BirdProfile->FlightMode)
    {
    case EGamePlatformBirdFlockFlightMode::Traverse:
    {
        // 掠过型路径：超出包围区域后从远端重入，编辑器可降低圈半径快速复核边界。
        const double Span = 4.0 * BirdProfile->RadiusCm;
        double Progress = FMath::Fmod(
            static_cast<double>(Bird.HeadingRadians) * Span / UE_TWO_PI +
            AtSeconds * Speed, Span);
        if (Progress < 0.0) Progress += Span;
        Position.X = Progress - Span * 0.5;
        Position.Y = Radius * FMath::Sin(Phase * 2.5) * 0.75;
        break;
    }
    case EGamePlatformBirdFlockFlightMode::FigureEight:
    {
        // 八字路径连续且天然回环，避免随机游走的速度与位置漂移。
        const double Angle = Phase + Step;
        Position.X = Radius * FMath::Sin(Angle);
        Position.Y = Radius * 0.56 * FMath::Sin(Angle * 2.0);
        break;
    }
    case EGamePlatformBirdFlockFlightMode::Orbit:
    default:
    {
        const double Angle = Phase + Step;
        const float RadiusWobble = 1.0f + 0.055f * FMath::Sin(AtSeconds * 0.42 + Phase);
        Position.X = Radius * RadiusWobble * FMath::Cos(Angle);
        Position.Y = Radius * RadiusWobble * FMath::Sin(Angle);
        break;
    }
    }
    // 不把风速当成无限积分位移；轻微横向风扰动不应迫使鸟群离开安全表现区域。
    const FVector WindUnit = WindDirection.GetSafeNormal();
    Position += WindUnit * (BirdProfile->WindInfluenceCm *
        FMath::Sin(AtSeconds * 0.31 + Phase * 1.17));
    Position.Z = BirdProfile->FlightHeightCm +
        Bird.HeightFactor * BirdProfile->HeightVariationCm +
        0.25f * BirdProfile->HeightVariationCm *
        FMath::Sin(AtSeconds * 0.8 + Phase * 1.9);
    return Position;
}

void AGamePlatformBirdFlockActor::UpdateBirdTransforms(double AtSeconds)
{
    TRACE_CPUPROFILER_EVENT_SCOPE(GamePlatformBirdFlockUpdateInstances);
    if (!BirdProfile || !BirdInstances || Birds.IsEmpty())
    {
        return;
    }
    const int32 Count = Birds.Num();
    for (int32 Index = 0; Index < Count; ++Index)
    {
        const FBirdSample& Bird = Birds[Index];
        const FVector Position = SampleLocalPosition(Bird, AtSeconds);
        // 有限差分仅用于朝向与轻微侧倾，不会产生服务器或物理负载。
        const FVector Direction = (
            SampleLocalPosition(Bird, AtSeconds + 0.04) -
            SampleLocalPosition(Bird, AtSeconds - 0.04)).GetSafeNormal();
        FRotator Rotation = Direction.IsNearlyZero() ? FRotator::ZeroRotator : Direction.Rotation();
        if (BirdProfile->FlightMode == EGamePlatformBirdFlockFlightMode::Orbit)
        {
            Rotation.Roll = FMath::Clamp(10.0f * Bird.SpeedFactor, 0.0f, 20.0f);
        }
        const FTransform Transform(
            Rotation, Position, FVector(Bird.ScaleFactor));
        // 只有最后一个实例触发一次渲染状态更新，避免512次重复提交。
        BirdInstances->UpdateInstanceTransform(Index, Transform, false, Index == Count - 1, true);
    }
}

void AGamePlatformBirdFlockActor::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!CanRenderLocally() || !BirdProfile || Birds.IsEmpty())
    {
        SetActorTickEnabled(false);
        return;
    }
    SimulationTimeSeconds += DeltaSeconds;
    UpdateAccumulatorSeconds += DeltaSeconds;
    const float Rate = FMath::Max(5.0f, BirdProfile->UpdateRateHz);
    if (UpdateAccumulatorSeconds < 1.0f / Rate)
    {
        return;
    }
    UpdateAccumulatorSeconds = 0.0f;
    // 背景/非可见区域继续推进时钟，但暂停CPU矩阵批量更新，回到视野时无时间滞后。
    if (BirdInstances->WasRecentlyRendered(0.5f))
    {
        UpdateBirdTransforms(SimulationTimeSeconds);
    }
}

void AGamePlatformBirdFlockActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClearFlock();
    Super::EndPlay(EndPlayReason);
}

bool AGamePlatformBirdFlockActor::SetBirdProfile(UGamePlatformBirdFlockProfile* NewProfile)
{
    if (!NewProfile)
    {
        return false;
    }
    FString Reason;
    if (!NewProfile->Validate(Reason))
    {
        UE_LOG(LogTemp, Warning, TEXT("切换鸟类预设失败：%s"), *Reason);
        return false;
    }
    BirdProfile = NewProfile;
    return RefreshFlock();
}

bool AGamePlatformBirdFlockActor::SetBirdCount(int32 NewCount)
{
    if (NewCount < 0 || NewCount > FGamePlatformBirdFlockMath::HardMaxBirdCount)
    {
        return false;
    }
    BirdCountOverride = NewCount;
    return RefreshFlock();
}

bool AGamePlatformBirdFlockActor::SetQualityFraction(float NewFraction)
{
    if (!FMath::IsFinite(NewFraction) || NewFraction < 0.0f || NewFraction > 1.0f)
    {
        return false;
    }
    QualityFraction = NewFraction;
    return RefreshFlock();
}

void AGamePlatformBirdFlockActor::SetWindDirection(const FVector& NewDirection)
{
    if (!NewDirection.ContainsNaN())
    {
        WindDirection = NewDirection.GetSafeNormal();
    }
}

int32 AGamePlatformBirdFlockActor::GetEffectiveBirdCount() const
{
    return BirdInstances ? BirdInstances->GetInstanceCount() : 0;
}

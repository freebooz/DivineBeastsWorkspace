// 世界水域注册来源为权威地图Actor；服务器和客户端共享静态几何，不复制每帧波纹。
#include "Actors/GamePlatformWaterRegion.h"
#include "Subsystems/GamePlatformWaterWorldSubsystem.h"

#include "Components/BoxComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AGamePlatformWaterRegion::AGamePlatformWaterRegion()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false; // 一期地图静态水域双端Cook一致；动态水位另设计权威复制。
    WaterRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WaterRoot"));
    SetRootComponent(WaterRoot);
    WaterBounds = CreateDefaultSubobject<UBoxComponent>(TEXT("WaterBounds"));
    WaterBounds->SetupAttachment(WaterRoot);
    WaterBounds->SetBoxExtent(FVector(800.f, 800.f, 300.f));
    WaterBounds->SetRelativeLocation(FVector(0.f, 0.f, -300.f));
    WaterBounds->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    WaterBounds->SetGenerateOverlapEvents(false);
    WaterBounds->SetCanEverAffectNavigation(false);
}

bool AGamePlatformWaterRegion::IsWaterRegionValid() const
{
    const FVector Extents = WaterBounds ? WaterBounds->GetUnscaledBoxExtent() : FVector::ZeroVector;
    return Region.IsValid() && IsValid(WaterBounds) &&
        Extents.GetMin() > 0.f && FMath::IsFinite(GetActorLocation().Z);
}

bool AGamePlatformWaterRegion::SampleRegion(
    const FVector& WorldPosition, FGamePlatformWaterSample& OutSample) const
{
    OutSample = FGamePlatformWaterSample();
    if (!IsWaterRegionValid())
        return false;
    const float SurfaceZ = static_cast<float>(GetActorLocation().Z) + Region.SurfaceOffsetCm;
    return FGamePlatformWaterMath::SampleWaterVolume(
        WaterBounds->GetComponentTransform(), WaterBounds->GetUnscaledBoxExtent(),
        SurfaceZ, WorldPosition, Region, OutSample);
}

void AGamePlatformWaterRegion::OnConstruction(const FTransform& Transform)
{
    Super::OnConstruction(Transform);
    // 编辑器中不产生资源、修改水位或引擎WaterBody，只校验并在PIE注册。
}

void AGamePlatformWaterRegion::BeginPlay()
{
    Super::BeginPlay();
    if (UWorld* World = GetWorld())
        if (UGamePlatformWaterWorldSubsystem* Water = World->GetSubsystem<UGamePlatformWaterWorldSubsystem>())
            Water->RegisterRegion(this);
}

void AGamePlatformWaterRegion::EndPlay(const EEndPlayReason::Type Reason)
{
    if (UWorld* World = GetWorld())
        if (UGamePlatformWaterWorldSubsystem* Water = World->GetSubsystem<UGamePlatformWaterWorldSubsystem>())
            Water->UnregisterRegion(this);
    Super::EndPlay(Reason);
}

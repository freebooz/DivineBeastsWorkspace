#include "Actors/GamePlatformWaterPhysicsVolume.h"
#include "Actors/GamePlatformWaterRegion.h"
#include "Subsystems/GamePlatformWaterWorldSubsystem.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"

AGamePlatformWaterPhysicsVolume::AGamePlatformWaterPhysicsVolume()
{
    PrimaryActorTick.bCanEverTick = false;
    bReplicates = false;
    bWaterVolume = true;
    bPhysicsOnContact = false;
}

bool AGamePlatformWaterPhysicsVolume::IsOverlapInVolume(const USceneComponent& Component) const
{
    const UWorld* World = GetWorld();
    const auto* Region = WaterRegion.Get();
    if (!IsValid(World) || World->bIsTearingDown || !World->IsGameWorld() || Component.GetWorld() != World ||
        Component.GetComponentLocation().ContainsNaN() || !IsValid(Region) || Region->GetWorld() != World ||
        Region->WaterPhysicsVolume != this || !Region->IsWaterRegionValid() || !Super::IsOverlapInVolume(Component))
        return false;
    const auto* Water = World->GetSubsystem<UGamePlatformWaterWorldSubsystem>();
    FGamePlatformWaterSample Sample;
    // SampleRegion使用AVolume非虚的EncompassesPoint，只查询几何，不会重入本资格回调。
    return Water && Water->IsRegionRegistered(Region) && Water->QueryWaterAtLocation(Component.GetComponentLocation(), Sample) &&
        Sample.bSwimmable && Sample.RegionId == Region->Region.RegionId;
}

#include "Marks/GamePlatformSurfaceMarkBlueprintLibrary.h"
#include "Marks/IGamePlatformSurfaceMarkService.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

namespace
{
UWorld* ResolveMarkWorld(const UObject* Context)
{
    return GEngine && IsValid(Context) ?
        GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
}
}

FGamePlatformSurfaceMarkResult UGamePlatformSurfaceMarkBlueprintLibrary::SubmitSurfaceMark(
    const UObject* WorldContextObject, const FGamePlatformSurfaceMarkRequest& Request)
{
    UWorld* World = ResolveMarkWorld(WorldContextObject);
    if (!IsValid(World)) return {};
    auto* Service = IGamePlatformSurfaceMarkService::Get(*World);
    return Service ? Service->Submit(Request) : FGamePlatformSurfaceMarkResult();
}

FGamePlatformSurfaceMarkResult UGamePlatformSurfaceMarkBlueprintLibrary::SubmitSurfaceMarkFromHit(
    const UObject* WorldContextObject, EGamePlatformSurfaceMarkKind Kind,
    const FHitResult& Hit, FVector AttackDirection, FGuid EventId, float SizeScale)
{
    FGamePlatformSurfaceMarkRequest Request;
    Request.Kind = Kind;
    Request.EventId = EventId;
    Request.Location = Hit.ImpactPoint;
    Request.Normal = Hit.ImpactNormal;
    Request.Direction = AttackDirection;
    Request.SurfaceType = UPhysicalMaterial::DetermineSurfaceType(Hit.PhysMaterial.Get());
    Request.HitComponent = Hit.GetComponent();
    Request.SizeScale = SizeScale;
    return SubmitSurfaceMark(WorldContextObject, Request);
}

bool UGamePlatformSurfaceMarkBlueprintLibrary::RefreshSurfaceMarkCatalog(const UObject* WorldContextObject)
{
    UWorld* World = ResolveMarkWorld(WorldContextObject);
    if (!IsValid(World)) return false;
    auto* Service = IGamePlatformSurfaceMarkService::Get(*World);
    return Service && Service->RefreshCatalog();
}

bool UGamePlatformSurfaceMarkBlueprintLibrary::GetSurfaceMarkCounts(
    const UObject* WorldContextObject, int32& Active, int32& Pending)
{
    Active = Pending = 0;
    UWorld* World = ResolveMarkWorld(WorldContextObject);
    if (!IsValid(World)) return false;
    auto* Service = IGamePlatformSurfaceMarkService::Get(*World);
    if (!Service) return false;
    Service->GetCounts(Active, Pending);
    return true;
}

// 蓝图调用只负责解析世界并转发到低层Surface服务，不维护第二份状态。
#include "Blueprint/GamePlatformSurfaceBlueprintLibrary.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interfaces/IGamePlatformSurfaceService.h"

namespace
{
FGamePlatformSurfaceUpdateResult MakeInvalidWorldResult()
{
    FGamePlatformSurfaceUpdateResult Result;
    Result.Status = EGamePlatformSurfaceUpdateStatus::InvalidWorld;
    return Result;
}

UWorld* ResolveSurfaceWorld(const UObject* WorldContextObject)
{
    return GEngine
        ? GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull)
        : nullptr;
}
}

FGamePlatformSurfaceUpdateResult UGamePlatformSurfaceBlueprintLibrary::ApplyEnvironmentState(
    const UObject* WorldContextObject,
    const FGamePlatformSurfaceEnvironmentState& State)
{
    UWorld* World = ResolveSurfaceWorld(WorldContextObject);
    IGamePlatformSurfaceService* Service = World ? IGamePlatformSurfaceService::Get(*World) : nullptr;
    return Service ? Service->ApplyEnvironmentState(State) : MakeInvalidWorldResult();
}

bool UGamePlatformSurfaceBlueprintLibrary::GetEnvironmentState(
    const UObject* WorldContextObject,
    FGamePlatformSurfaceEnvironmentState& OutState,
    int32& OutRevision)
{
    UWorld* World = ResolveSurfaceWorld(WorldContextObject);
    IGamePlatformSurfaceService* Service = World ? IGamePlatformSurfaceService::Get(*World) : nullptr;
    if (!Service)
    {
        OutState = FGamePlatformSurfaceEnvironmentState();
        OutRevision = 0;
        return false;
    }

    OutState = Service->GetEnvironmentState();
    OutRevision = Service->GetRevision();
    return true;
}

FGamePlatformSurfaceUpdateResult UGamePlatformSurfaceBlueprintLibrary::RefreshMaterialBinding(
    const UObject* WorldContextObject)
{
    UWorld* World = ResolveSurfaceWorld(WorldContextObject);
    IGamePlatformSurfaceService* Service = World ? IGamePlatformSurfaceService::Get(*World) : nullptr;
    return Service ? Service->RefreshMaterialBinding() : MakeInvalidWorldResult();
}

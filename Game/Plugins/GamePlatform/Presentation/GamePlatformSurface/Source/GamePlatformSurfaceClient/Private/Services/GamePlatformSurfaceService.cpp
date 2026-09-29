// Surface低层服务定位入口；调用方只依赖公开接口，不需要访问私有子系统类型。
#include "Interfaces/IGamePlatformSurfaceService.h"

#include "Engine/World.h"
#include "Subsystems/GamePlatformSurfaceWorldSubsystem.h"

IGamePlatformSurfaceService* IGamePlatformSurfaceService::Get(UWorld& World)
{
    return World.GetSubsystem<UGamePlatformSurfaceWorldSubsystem>();
}

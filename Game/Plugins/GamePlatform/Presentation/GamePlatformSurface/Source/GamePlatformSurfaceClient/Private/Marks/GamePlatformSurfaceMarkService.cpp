// 与环境状态服务相同的世界定位模式：只返回客户端已有子系统。
#include "Marks/IGamePlatformSurfaceMarkService.h"
#include "Marks/GamePlatformSurfaceMarkWorldSubsystem.h"
#include "Engine/World.h"

IGamePlatformSurfaceMarkService* IGamePlatformSurfaceMarkService::Get(UWorld& World)
{
    return World.GetSubsystem<UGamePlatformSurfaceMarkWorldSubsystem>();
}

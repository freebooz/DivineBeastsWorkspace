#pragma once
#include "Interfaces/IGamePlatformGameplayService.h"
#include "Interfaces/IGamePlatformGameplayAdmissionSink.h"

/** 显式世界的类型化入口；无对象扫描、GWorld或第零玩家假定。 */
namespace GamePlatformGameplayServices
{
    /** 返回本世界GameState上的已有组件；未创建或类型不匹配时为空。只限游戏线程。 */
    GAMEPLATFORMGAMEPLAY_API IGamePlatformGameplayService* Get(UWorld& World);
    /** 仅权威Game/PIE世界返回已有GameMode接收口；远程客户端必为空。 */
    GAMEPLATFORMGAMEPLAY_API IGamePlatformGameplayAdmissionSink* GetAdmissionSink(UWorld& World);
}

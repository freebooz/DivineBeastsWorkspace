#pragma once
#include "Interfaces/IGamePlatformLoadingTask.h"
/** 组合根将工厂以WorldReadiness注册到本实例Loading；Loading不反向依赖World。 */
namespace GamePlatformWorldServices
{
    /** 创建独占任务；Start绑定当时实例的当前世界/代次，跨图须创建新任务。 */
    GAMEPLATFORMWORLD_API TUniquePtr<IGamePlatformLoadingTask> CreateReadinessTask();
}

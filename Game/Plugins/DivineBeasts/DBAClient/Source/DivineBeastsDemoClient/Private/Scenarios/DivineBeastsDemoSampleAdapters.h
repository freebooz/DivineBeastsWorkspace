// SSLM三类核心样板工厂：仅模块内组合，真实UI/项目鸟群/中央资源服务各有独立所有权。
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"
namespace DivineBeasts::Demo
{
    /** 需要已加载的真实ResourceBarWidget，不替代Monolith资产。 */
    TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateResourceBarScenario();
    /** 只复用本世界已存在的正式SceneComponent和既有操作。 */
    TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateBirdFlockScenario();
    /** 实际请求Engine Cube，不提供内存加载替身。 */
    TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateDataLeaseScenario();
}

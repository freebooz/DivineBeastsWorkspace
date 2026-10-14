// SSLM项目开发逻辑身份样板：Private组合真实平台Core值能力，无资源/用户/网络状态所有权。
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"
namespace DivineBeasts::Demo
{
    /** 游戏线程按Namespace/Name/LogicalVersion三个类型化值执行真实创建/规范化/回环/哈希。
     * 失败清空OutCanonical，保留平台拒绝原因；不把非法开发夹具修成合法生产身份。
     */
    FGamePlatformResult EvaluateCoreIdentity(const TArray<FDivineBeastsDemoParameterValue>& Values, FString& OutCanonical);
    /** 每个世界运行创建独立适配实例；Prepare绑定Token、Stop失活，无全局注册或持久复位。 */
    TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateCoreIdentityScenario();
}

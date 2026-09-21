#pragma once

#include "Interfaces/IGamePlatformLoadingTask.h"

class UDBAPCGResultComponent;

namespace DBAPCGLoading
{
/** 2026-09-21交接断点：用户转入Input，本桥接停止在未装配状态，不继续修改其他插件。
 * 已写入真实Loading适配及世界结果组件；未改Build.cs、uproject、配置、Coordinator或WorldBootstrap。
 * 生命周期编译期回归经MSVC /std:c++17 /W4 /WX /Zs通过；不是UE反射/链接/联机/清理实测。
 * 续接位置：WorldBootstrap::AdvanceWorld在真实区域登记成功后创建组件、绑定世界/区域，
 * 在StartLoadingOperation前注册本工厂并添加Profile任务；WorldReadiness可以依赖该任务。
 * 操作释放只终结等待；组件须由宿主Actor继续持有，区域失效交PCG处理，显式结束再释放组件结果。
 * 尚需：主模块PCG依赖、插件启用、真实Profile与Cook收集、目录规划登记，以及UE三目标验证。
 * 注意：Private中新cpp会被UBT发现；未装配不等于编译隔离，缺少PCG模块依赖时不能宣称主工程可构建。
 */

/** 返回仅捕获弱组件的新任务工厂，不自动注册、不启动操作、不持有其他GameInstance对象。
 * 组合根以DBAPCGGeneration等唯一键调用Loading.RegisterTaskFactory，并保留注册句柄。
 * TaskSpec.Data.DefinitionId提供PCG Profile身份；其他Data字段不用于本任务，PCG服务决定类及Bundles。
 * 每个同时活跃的任务使用独立世界组件；组件必须先BindWorldRegion，且由世界Actor持有。
 * 成功任务Release/析构保留组件的输出；未成功任务释放自己的精确请求，失败码保留在PCG快照。
 * WorldReadiness任务可依赖本任务，反向依赖会形成业务循环；本工厂不声明任何Session准入或WorldReady。
 * 接入还需主模块PrivateDependencyModuleNames加入GamePlatformPCG，主工程启用该插件。
 * 工厂解绑必须在Loading操作释放后；结果组件应持续存活到区域结束或显式ReleaseGeneration。
 */
FGamePlatformLoadingTaskFactory CreateTaskFactory(UDBAPCGResultComponent& ResultOwner);
}

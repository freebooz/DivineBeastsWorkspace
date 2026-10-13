// SSLM开发适配注册表；游戏实例私有，拥有受控工厂，不拥有世界或活跃适配实例。
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"

namespace DivineBeasts::Demo
{
/** 同一实例内无重复身份，完整注册句柄禁止跨实例/过期注销。 */
class FAdapterRegistry
{
public:
    /** 每个实例新作用域，不使用静态全局计数或进程缓存。 */
    FAdapterRegistry();
    /** 接受有效ID和工厂；重复返回无效句柄，不覆盖已有注册。 */
    FDivineBeastsDemoAdapterRegistrationHandle Register(FName Id, FDivineBeastsDemoAdapterFactory Factory);
    /** 仅完整当前句柄可注销；错误/旧句柄返回失败。 */
    FGamePlatformResult Unregister(const FDivineBeastsDemoAdapterRegistrationHandle& Handle);
    /** 只查询是否注册，不能据此宣称实例准备成功。 */
    bool Contains(FName Id) const;
    /** 为指定世界调用受控工厂；空实例明确表示创建失败。 */
    TUniquePtr<IDivineBeastsDemoScenarioAdapter> Create(FName Id, UWorld& World) const;
    /** 完整句柄查询注册身份，供提供者撤销时停止关联活跃案例。 */
    FName FindIdentity(const FDivineBeastsDemoAdapterRegistrationHandle& Handle) const;
    /** 实例退出撤销全部注册；运行实例需由世界先清理。 */
    void Reset();
private:
    /** 工厂调用持共享记录，避免工厂重入注销导致本次调用栈悬空。 */
    struct FEntry { FDivineBeastsDemoAdapterRegistrationHandle Handle; FDivineBeastsDemoAdapterFactory Factory; };
    TMap<FName, TSharedPtr<FEntry>> Entries;
    FGuid InstanceId;
    uint64 Generation = 0;
};
}

// SSLM项目开发案例适配合同；只消费低层公开能力，不公开注册表/缓存或业务权威。
#pragma once
#include "Contracts/DivineBeastsDemoRunTypes.h"

/** 单次运行拥有一个适配实例；限游戏线程，析构需立即撤销本实例异步/租约，不能调用旧世界对象。 */
class DIVINEBEASTSDEMOCLIENT_API IDivineBeastsDemoScenarioAdapter
{
public:
    virtual ~IDivineBeastsDemoScenarioAdapter() = default;
    /** 保存输入值副本并准备本案例资源；同步/异步均至多回调一次，失败也必须给具名结果。 */
    virtual void Prepare(const FDivineBeastsDemoScenarioContext& Context, FDivineBeastsDemoOperationCompletion Completion) = 0;
    /** 只执行声明的真实公开操作；Parameters按定义数组顺序传值，不能执行脚本文本。 */
    virtual void Execute(FName OperationId, const TArray<FDivineBeastsDemoParameterValue>& Parameters,
        FDivineBeastsDemoOperationCompletion Completion) = 0;
    /** 拒绝新操作后恢复/释放本案例所有权，失败不能伪装正常停止；支持准备中取消。 */
    virtual void Stop(FDivineBeastsDemoOperationCompletion Completion) = 0;
};

/** 受控工厂；每世界创建独立实例，空返回表示不能创建，不能共享进程单例。 */
using FDivineBeastsDemoAdapterFactory = TFunction<TUniquePtr<IDivineBeastsDemoScenarioAdapter>(UWorld&)>;

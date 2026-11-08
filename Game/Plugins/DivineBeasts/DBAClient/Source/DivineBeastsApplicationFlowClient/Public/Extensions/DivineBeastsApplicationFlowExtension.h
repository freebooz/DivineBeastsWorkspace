#pragma once

#include "CoreMinimal.h"
#include "Flow/DivineBeastsFlowTypes.h"

class UDivineBeastsApplicationFlowSubsystem;
class UWorld;

/** 恢复票据只在游戏线程的完成调用中移交。撤销函数与本次RequestWorld节点同寿命。 */
using FDivineBeastsWorldRecoveryCompletion = TFunction<void(bool, FDivineBeastsWorldAssignmentPayload)>;
using FDivineBeastsCancelWorldRecovery = TFunction<void()>;

/**
 * IDivineBeastsApplicationFlowExtension（神兽联盟应用流程扩展接口）。
 * Arena等可选模块由主工程组合根注册；核心ApplicationFlowClient不依赖ArenaClient。
 */
class DIVINEBEASTSAPPLICATIONFLOWCLIENT_API IDivineBeastsApplicationFlowExtension
{
public:
    virtual ~IDivineBeastsApplicationFlowExtension() = default;

    virtual void OnEnteredInWorld(
        UDivineBeastsApplicationFlowSubsystem& Flow) = 0;

    virtual void OnLeavingInWorld(
        UDivineBeastsApplicationFlowSubsystem& Flow) = 0;

    /** 可选世界提供者只读采集本世界的真实绑定/必要资源/服务器激活事实。
     * false表示不拥有该世界；true仍须由Flow独立核对Session准入和真实WorldDefinition。
     * 变化由提供者通知Flow，不能在此启动轮询、授予资格或提前完成加载。 */
    virtual bool TryObserveWorldReadiness(UWorld& World, bool& bPawnBound, bool& bResourcesPrepared, bool& bGameplayActive) const { return false; }

    /** 从权威扩展取得新准入。返回空函数表示本扩展未受理，核心仍校验角色、会话和目标。 */
    virtual FDivineBeastsCancelWorldRecovery TryRequestWorldRecovery(
        const FDivineBeastsWorldAssignmentSummary& Previous, FDivineBeastsWorldRecoveryCompletion Completion)
    {
        return {};
    }

    /** 兼容蓝图入口。由比赛扩展确认当前可退场，并提交正常的可信迁移命令。 */
    virtual bool RequestPostMatchReturn() { return false; }
};

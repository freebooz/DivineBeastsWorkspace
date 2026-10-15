#pragma once

#include "CoreMinimal.h"

class UDivineBeastsApplicationFlowSubsystem;
class UWorld;

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
};

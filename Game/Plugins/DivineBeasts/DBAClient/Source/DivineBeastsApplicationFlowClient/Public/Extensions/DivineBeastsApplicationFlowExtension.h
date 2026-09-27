#pragma once

#include "CoreMinimal.h"

class UDivineBeastsApplicationFlowSubsystem;

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
};

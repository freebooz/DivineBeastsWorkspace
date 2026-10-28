// SSLM开发页平台基础/服务适配；仅Private使用，真实操作由平台公开实现执行。
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"
class UDivineBeastsDemoCatalogSubsystem;
namespace DivineBeasts::Demo
{
    /** 只注册封闭平台能力，不替换已有CoreIdentity/DataLease。返回句柄归Catalog实例。 */
    TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterPlatformServiceScenarios(UDivineBeastsDemoCatalogSubsystem& Catalog);
    /** 独立实例绑定构造时的世界；未知功能在Prepare失败关闭。 */
    TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePlatformServiceScenario(UWorld& World,FName FeatureId);
    /** 编译期封闭签名，只描述允许的字段/操作，不复制作者默认值或业务规则。 */
    bool GetPlatformManualSignature(FName Feature,TArray<FName>& Names,TArray<EDivineBeastsDemoParameterType>& Types,TArray<FName>& Operations);
    bool ValidatePlatformManualContract(const FDivineBeastsDemoScenarioDefinition& Scenario);
    /** 原生测试与人工操作共享真实Core值调用，不能用自动测试替代人工页。 */
    FGamePlatformResult EvaluateManualCoreVersion(const TArray<FDivineBeastsDemoParameterValue>& Values);
}


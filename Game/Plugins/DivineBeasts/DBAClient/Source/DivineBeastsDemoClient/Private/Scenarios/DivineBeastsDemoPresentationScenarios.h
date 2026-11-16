// 项目私有人工表现适配；所有执行仍调用平台公开服务，不能作为玩法入口。
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"
#include "Types/GamePlatformSFXTypes.h"
#include "Styling/GamePlatformUIThemeTypes.h"
class UDivineBeastsDemoCatalogSubsystem;
namespace DivineBeasts::Demo
{
/** 返回编译白名单；供作者核验和根组合注册，不按文本反射执行。 */
TArray<FName> PresentationManualFeatures();
TArray<FName> PresentationManualOperations(FName Feature);
bool GetPresentationManualParameterSignature(FName Feature,TArray<FName>& Names,TArray<EDivineBeastsDemoParameterType>& Types);
bool ValidatePresentationManualContract(const FDivineBeastsDemoScenarioDefinition& Definition);
FGamePlatformResult ValidatePresentationManualValues(const FDivineBeastsDemoScenarioDefinition& Definition,const TArray<FDivineBeastsDemoParameterValue>& Values);
bool CanBeginPresentationThemeFixture(const FGamePlatformUIThemeSnapshot& Snapshot);
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreatePresentationManualScenario(UWorld& World, FName Feature);
TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterPresentationManualScenarios(UDivineBeastsDemoCatalogSubsystem& Catalog);
/** 完整SFX句柄比较，包含World；RequestId相等本身不足以消费终态。 */
bool PresentationManualSameSFXHandle(const FGamePlatformSFXHandle& A, const FGamePlatformSFXHandle& B);
}

// 项目私有世界人工页合同；只调用平台现有服务，端侧桥条件编译。
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"
class UDivineBeastsDemoCatalogSubsystem;
namespace DivineBeasts::Demo {
TArray<FName> WorldManualFeatures();
TArray<FName> WorldManualOperations(FName Feature);
bool ValidateWorldManualContract(const FDivineBeastsDemoScenarioDefinition& D);
FGamePlatformResult ValidateWorldManualValues(const FDivineBeastsDemoScenarioDefinition& D,const TArray<FDivineBeastsDemoParameterValue>& Values);
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateWorldManualScenario(UWorld& World,FName Feature);
TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterWorldManualScenarios(UDivineBeastsDemoCatalogSubsystem& Catalog);
}

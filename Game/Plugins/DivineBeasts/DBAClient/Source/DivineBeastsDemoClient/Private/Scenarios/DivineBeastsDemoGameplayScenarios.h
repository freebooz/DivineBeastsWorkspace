// Project-private adapters for the existing platform execution chains. GT only.
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"
#include "GameplayTagContainer.h"
class UDivineBeastsDemoCatalogSubsystem;
namespace DivineBeasts::Demo {
/** 只在Runtime模块完成加载后的操作/合同读取时取得既有标签，不建立静态缓存。 */
FGameplayTag GameplayManualInputTag();
TArray<FName> GameplayManualFeatures();
FDivineBeastsDemoScenarioDefinition GameplayManualDefinition(FName Feature);
bool ValidateGameplayManualContract(const FDivineBeastsDemoScenarioDefinition& Definition);
FGamePlatformResult ValidateGameplayManualValues(const FDivineBeastsDemoScenarioDefinition&,const TArray<FDivineBeastsDemoParameterValue>&);
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateGameplayManualScenario(UWorld&,FName Feature);
TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterGameplayManualScenarios(UDivineBeastsDemoCatalogSubsystem&);
}

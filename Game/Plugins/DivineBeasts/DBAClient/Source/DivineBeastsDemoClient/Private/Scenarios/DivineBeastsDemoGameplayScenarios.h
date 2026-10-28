// Project-private adapters for the existing platform execution chains. GT only.
#pragma once
#include "API/IDivineBeastsDemoScenarioAdapter.h"
class UDivineBeastsDemoCatalogSubsystem;
namespace DivineBeasts::Demo {
TArray<FName> GameplayManualFeatures();
FDivineBeastsDemoScenarioDefinition GameplayManualDefinition(FName Feature);
bool ValidateGameplayManualContract(const FDivineBeastsDemoScenarioDefinition& Definition);
FGamePlatformResult ValidateGameplayManualValues(const FDivineBeastsDemoScenarioDefinition&,const TArray<FDivineBeastsDemoParameterValue>&);
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateGameplayManualScenario(UWorld&,FName Feature);
TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterGameplayManualScenarios(UDivineBeastsDemoCatalogSubsystem&);
}

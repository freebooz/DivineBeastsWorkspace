#include "Scenarios/DivineBeastsDemoWorldScenarios.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
namespace DivineBeasts::Demo {
TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterWorldManualScenarios(UDivineBeastsDemoCatalogSubsystem& C){
 TArray<FDivineBeastsDemoAdapterRegistrationHandle> Owned;
 for(FName F:WorldManualFeatures()){
  auto H=C.RegisterScenarioAdapter(FName(*(TEXT("PlatformWorld.")+F.ToString())),[F](UWorld& W){return CreateWorldManualScenario(W,F);});
  if(!H.IsValid()){for(const auto& Prior:Owned)C.UnregisterScenarioAdapter(Prior);return {};}
  Owned.Add(H);
 }return Owned;
}
}

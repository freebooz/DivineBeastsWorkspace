#include "Scenarios/DivineBeastsDemoPresentationScenarios.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
namespace DivineBeasts::Demo
{
TArray<FDivineBeastsDemoAdapterRegistrationHandle> RegisterPresentationManualScenarios(UDivineBeastsDemoCatalogSubsystem& Catalog)
{
    TArray<FDivineBeastsDemoAdapterRegistrationHandle> Handles;
    for(FName Feature:PresentationManualFeatures())
    {
        const FName Adapter(*(TEXT("PlatformPresentation.")+Feature.ToString()));
        const auto H=Catalog.RegisterScenarioAdapter(Adapter,[Feature](UWorld& W){return CreatePresentationManualScenario(W,Feature);});
        if(!H.IsValid()){for(const auto& Own:Handles)Catalog.UnregisterScenarioAdapter(Own);return {};}
        Handles.Add(H);
    }
    return Handles;
}
}

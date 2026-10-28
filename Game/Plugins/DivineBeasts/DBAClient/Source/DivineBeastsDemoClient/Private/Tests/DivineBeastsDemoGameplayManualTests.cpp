// Test-first candidate; root compiles/runs after integration. No UE execution claimed here.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Scenarios/DivineBeastsDemoGameplayScenarios.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include <limits>
using namespace DivineBeasts::Demo;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAGameplayContractTest,"DivineBeasts.PluginDemos.Gameplay.Contract",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAGameplayContractTest::RunTest(const FString&){
 TestEqual(TEXT("24 audited candidate groups remain distinct"),GameplayManualFeatures().Num(),24);
 for(auto F:GameplayManualFeatures()){
  auto D=GameplayManualDefinition(F);TestTrue(TEXT("exact author contract"),ValidateGameplayManualContract(D));
  D.Operations.Add(TEXT("Console"));TestFalse(TEXT("arbitrary execution refused"),ValidateGameplayManualContract(D));
  D=GameplayManualDefinition(F);D.Operations.Last()=D.Operations[0];TestFalse(TEXT("duplicates cannot replace Stop"),ValidateGameplayManualContract(D));
  D=GameplayManualDefinition(F);D.AdapterId=TEXT("PlatformWorld.Foreign");TestFalse(TEXT("foreign adapter refused"),ValidateGameplayManualContract(D));
 }
 return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAGameplayValuesTest,"DivineBeasts.PluginDemos.Gameplay.ParameterBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAGameplayValuesTest::RunTest(const FString&){
 auto D=GameplayManualDefinition(TEXT("GamePlatformCombat.damage_heal"));TArray<FDivineBeastsDemoParameterValue> V;for(auto P:D.Parameters)V.Add(P.DefaultValue);
 TestTrue(TEXT("typed defaults valid"),ValidateGameplayManualValues(D,V).IsSuccess());
 V[0].NumberValue=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("NaN refuses before combat"),ValidateGameplayManualValues(D,V).IsSuccess());
 V[0]=D.Parameters[0].DefaultValue;V[0].NumberValue=100001;TestFalse(TEXT("bounded magnitude"),ValidateGameplayManualValues(D,V).IsSuccess());
 D=GameplayManualDefinition(TEXT("GamePlatformCharacter.initialize"));V.Reset();for(auto P:D.Parameters)V.Add(P.DefaultValue);
 V[0].IdentifierValue=TEXT("hero@1");TestFalse(TEXT("identifier cannot contain embedded version delimiter"),ValidateGameplayManualValues(D,V).IsSuccess());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAGameplaySourceTest,"DivineBeasts.PluginDemos.Gameplay.SourceIsolation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FDBAGameplaySourceTest::RunTest(const FString&){
 TStrongObjectPtr<UWorld>A(NewObject<UWorld>()),B(NewObject<UWorld>());auto X=CreateGameplayManualScenario(*A,TEXT("GamePlatformCombat.damage_heal"));
 FDivineBeastsDemoScenarioContext C;C.World=B.Get();C.Token.RunId=FGuid::NewGuid();C.Token.WorldId=FGuid::NewGuid();C.Token.Generation=1;C.Scenario=GameplayManualDefinition(TEXT("GamePlatformCombat.damage_heal"));
 int32 N=0;X->Prepare(C,[&](const auto&T,const auto&R){++N;TestTrue(TEXT("full token returned"),T==C.Token);TestFalse(TEXT("foreign source refused"),R.IsSuccess());});
 X->Stop([&](const auto&,const auto&R){++N;TestTrue(TEXT("no foreign resource to clean"),R.IsSuccess());});
 X->Execute(TEXT("Damage"),{},[&](const auto&,const auto&R){++N;TestFalse(TEXT("stopped source cannot revive"),R.IsSuccess());});TestEqual(TEXT("one completion per command"),N,3);return true;
}
#endif

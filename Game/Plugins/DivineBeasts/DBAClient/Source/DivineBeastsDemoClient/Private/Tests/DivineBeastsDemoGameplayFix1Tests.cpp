// Fix1 regressions are authored before the fixes; child cannot compile or run UE.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Scenarios/DivineBeastsDemoGameplayState.h"
using namespace DivineBeasts::Demo;
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayFix1ContractTest,"DivineBeasts.PluginDemos.Gameplay.Fix1.RequiredOperations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FGameplayFix1ContractTest::RunTest(const FString&){
 auto E=GameplayManualDefinition(TEXT("GamePlatformGameplay.experience"));TestTrue(TEXT("real experience start exists"),E.Operations.Contains(TEXT("BeginExperience")));TestTrue(TEXT("real drain exists"),E.Operations.Contains(TEXT("DrainExperience")));
 auto A=GameplayManualDefinition(TEXT("GamePlatformGameplay.admission_spawn"));for(auto O:{TEXT("PrepareGameplayHost"),TEXT("BeginExperience"),TEXT("SubmitFixtureAdmission"),TEXT("ConfirmFixturePreparation"),TEXT("RevokeFixtureAdmission")})TestTrue(TEXT("full offline fixture lifecycle uses platform APIs"),A.Operations.Contains(FName(O)));
 auto P=GameplayManualDefinition(TEXT("GamePlatformCombat.projectile"));for(auto O:{TEXT("LaunchMovingTarget"),TEXT("LaunchThenSwitch"),TEXT("LaunchThenInvalidate"),TEXT("LaunchStraight"),TEXT("LaunchBlocked")})TestTrue(TEXT("independent actionable native projectile case"),P.Operations.Contains(FName(O)));return true;
}
#endif

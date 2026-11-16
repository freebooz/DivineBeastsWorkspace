#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Bootstrap/World/DBAFoundationWorldBootstrap.h"
#include "Engine/GameInstance.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDBAWorldBootstrapSelectionTest,"DivineBeasts.Foundation.World.ManualHostExclusion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDBAWorldBootstrapSelectionTest::RunTest(const FString&)
{
 const FString Saved=FCommandLine::Get();ON_SCOPE_EXIT{FCommandLine::Set(*Saved);};
 auto* GI=NewObject<UGameInstance>();const auto* CDO=GetDefault<UDBAFoundationWorldBootstrap>();
 FCommandLine::Set(TEXT(""));TestFalse(TEXT("无标志不创建"),CDO->ShouldCreateSubsystem(GI));
 FCommandLine::Set(TEXT("-FoundationWorld"));TestTrue(TEXT("原Foundation独立工作流保留"),CDO->ShouldCreateSubsystem(GI));
 FCommandLine::Set(TEXT("-FoundationWorld -DBAPluginDemo -GamePlatformManualFixture"));TestTrue(TEXT("没有专用选择时不改变原Foundation"),CDO->ShouldCreateSubsystem(GI));
 FCommandLine::Set(TEXT("-FoundationWorld -DBAPluginDemo -GamePlatformManualFixture -DBAManualWorldHost"));TestFalse(TEXT("完整专用选择排除旧宿主"),CDO->ShouldCreateSubsystem(GI));
 return true;
}
#endif

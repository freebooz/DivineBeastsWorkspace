#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Scenarios/DivineBeastsDemoManualWorldHostSubsystem.h"
#include "Scenarios/DivineBeastsDemoWorldScenarios.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Misc/CommandLine.h"
#include "Misc/ScopeExit.h"
#include "HAL/PlatformTime.h"

// 缺任一显式选择标志都不能抢占旧Foundation工作流。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualWorldHostSelectionTest,"DivineBeasts.Demo.WorldHost.ExplicitSelection",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FManualWorldHostSelectionTest::RunTest(const FString&)
{
 const FString Old=FCommandLine::Get();ON_SCOPE_EXIT{FCommandLine::Set(*Old);};
 const auto* CDO=GetDefault<UDivineBeastsDemoManualWorldHostSubsystem>();
 UWorld* W=UWorld::CreateWorld(EWorldType::Game,false);
 for(const TCHAR* Flags:{TEXT(""),TEXT("-FoundationWorld"),TEXT("-DBAPluginDemo -GamePlatformManualFixture -FoundationWorld"),TEXT("-DBAPluginDemo -GamePlatformManualFixture -DBAManualWorldHost")}){
  FCommandLine::Set(Flags);TestFalse(TEXT("不完整选择不创建"),CDO->ShouldCreateSubsystem(W));
 }
 FCommandLine::Set(TEXT("-DBAPluginDemo -DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub -GamePlatformManualFixture -FoundationWorld -DBAManualWorldHost"));
 TestFalse(TEXT("错误地图拒绝，即使全部标志存在"),CDO->ShouldCreateSubsystem(W));
 W->DestroyWorld(false);return true;
}

class FManualWorldReadyWait final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;double Deadline=FPlatformTime::Seconds()+45;
public:explicit FManualWorldReadyWait(FAutomationTestBase& T):Test(&T){}
bool Update()override{
 UWorld* W=nullptr;if(GEngine)for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->WorldType==EWorldType::Game&&C.World()->GetNetMode()==NM_Standalone)W=C.World();
 auto* Host=W?W->GetSubsystem<UDivineBeastsDemoManualWorldHostSubsystem>():nullptr;
 if(!Test->TestNotNull(TEXT("必须真实Hub Standalone显式ManualWorldHost，不能NullRHI空世界跳过"),Host))return true;
 auto* S=IGamePlatformWorldService::Get(*W);auto Before=S->GetReadiness();
 if(Before.Context.ReadinessState==EGamePlatformWorldReadiness::Waiting&&FPlatformTime::Seconds()<Deadline)return false;
 Test->TestTrue(TEXT("真实世界初始化成功"),Before.Context.ReadinessState==EGamePlatformWorldReadiness::Ready);
 Test->TestTrue(TEXT("区域定义已加载且默认provider已登记"),Before.bRegionDefinitionsLoaded&&Before.bRequiredRegionsRegistered);
 Test->TestTrue(TEXT("无Session伪造"),Before.Context.AuthorityKind==EGamePlatformWorldAuthority::DevelopmentLocal&&Before.Context.ServerInstanceId.IsEmpty());
 FGamePlatformVersion Version;Version.Minor=1;
 Test->TestFalse(TEXT("再次初始化拒绝"),S->InitializeDevelopment(FPrimaryAssetId(TEXT("GamePlatformDefinition:world.demo.hub@1")),Version).IsSuccess());
 auto After=S->GetReadiness();Test->TestTrue(TEXT("完整世界代次保留"),After.Context.ContextGeneration==Before.Context.ContextGeneration);
 return true;
}
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualWorldHostLiveTest,"DivineBeasts.Demo.WorldLive.ManualHostReadyAndNoReinitialize",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FManualWorldHostLiveTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FManualWorldReadyWait(*this));return true;}
#endif

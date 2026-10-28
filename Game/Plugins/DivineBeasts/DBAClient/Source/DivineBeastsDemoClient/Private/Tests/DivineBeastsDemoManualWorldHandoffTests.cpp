#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Scenarios/DivineBeastsDemoManualWorldHostSubsystem.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Demo/DivineBeastsDemoCatalogSubsystem.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
namespace {
class FManualHandoffSequence final:public IAutomationLatentCommand
{
 FAutomationTestBase* Test;
 TWeakObjectPtr<UWorld> World;
 TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> Demo;
 FDivineBeastsDemoScenarioDefinition Regions,PCG;
 TWeakObjectPtr<AActor> ForeignOwner;
 FGamePlatformWorldRegistration Foreign;
 FGuid Generation;
 int32 Stage=0,Notifications=0;
 double Deadline;
 FDelegateHandle Subscription;
 bool Begin(const FDivineBeastsDemoScenarioDefinition& D){return Test->TestTrue(TEXT("从唯一Catalog真实开始"),Demo->BeginScenario(D));}
 void Stop(){Test->TestTrue(TEXT("真实Stop受理"),Demo->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("原生宿主交接回归"))));}
 bool Finish(){
  if(Demo.IsValid()){Demo->OnRunChanged.Remove(Subscription);if(Demo->HasActiveScenario())Stop();}
  if(World.IsValid())if(auto* S=IGamePlatformWorldService::Get(*World))if(Foreign.IsValid())S->UnregisterRegionProvider(Foreign);
  if(ForeignOwner.IsValid())ForeignOwner->Destroy();return true;
 }
public:
 FManualHandoffSequence(FAutomationTestBase& T):Test(&T),Deadline(FPlatformTime::Seconds()+45){}
 ~FManualHandoffSequence(){if(Demo.IsValid())Demo->OnRunChanged.Remove(Subscription);}
 bool Update() override{
  if(FPlatformTime::Seconds()>Deadline){Test->AddError(TEXT("真实宿主/资源/清理未达门禁，超时不跳过"));return Finish();}
  if(Stage==0){
   if(GEngine)for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->GetSubsystem<UDivineBeastsDemoManualWorldHostSubsystem>())World=C.World();
   if(!World.IsValid()){Test->AddError(TEXT("必须使用显式Hub Standalone ManualWorldHost Suite"));return true;}
   Demo=World->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();auto* Catalog=World->GetGameInstance()->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>();
   if(!Demo.IsValid()||!Catalog){Test->AddError(TEXT("实际Catalog/Demo缺失"));return true;}
   if(!Test->TestFalse(TEXT("不接管已有人工运行"),Demo->HasActiveScenario()))return true;
   if(!Catalog->FindScenarioDefinition(TEXT("GamePlatformWorld.regions.Manual"),Regions)||!Catalog->FindScenarioDefinition(TEXT("GamePlatformPCG.inspection.Manual"),PCG)){Test->AddError(TEXT("原生Catalog必须已冷读本次合同"));return true;}
   const auto Initial=IGamePlatformWorldService::Get(*World)->GetReadiness();
   if(Initial.Context.ReadinessState!=EGamePlatformWorldReadiness::Ready)return false;
   Generation=Initial.Context.ContextGeneration;Subscription=Demo->OnRunChanged.AddLambda([this](const auto&){++Notifications;});
   if(!Begin(Regions))return Finish();
   Test->TestEqual(TEXT("Preparing先同步撤销默认provider"),IGamePlatformWorldService::Get(*World)->GetReadiness().RegisteredRegionProviders,0);
   Stage=1;return false;
  }
  if(!World.IsValid()||!Demo.IsValid()){Test->AddError(TEXT("测试世界意外销毁，旧回调不能完成新世界"));return Finish();}
  auto* S=IGamePlatformWorldService::Get(*World);const auto V=Demo->GetRunSnapshot();
  if(V.State==EDivineBeastsDemoRunState::Failed){Test->AddError(TEXT("真实Run失败：")+V.OperationResult.Code.ToString()+TEXT(" / ")+V.CleanupResult.Code.ToString());return Finish();}
  if(Stage==1&&V.State==EDivineBeastsDemoRunState::Running){
   TArray<FDivineBeastsDemoParameterValue> Values;for(const auto& P:Regions.Parameters)Values.Add(P.DefaultValue);
   if(!Test->TestTrue(TEXT("真实Region注册命令受理"),Demo->RequestScenarioOperation(TEXT("RegisterRegion"),Values)))return Finish();Stage=2;return false;
  }
  if(Stage==2&&!V.bOperationPending&&V.CompletedOperations>0){
   Test->TestTrue(TEXT("Run provider成功，非宿主抢占"),V.OperationResult.IsSuccess());Test->TestEqual(TEXT("仅一份provider"),S->GetReadiness().RegisteredRegionProviders,1);Stop();Stage=3;return false;
  }
  if(Stage==3&&!Demo->HasActiveScenario()){
   Test->TestEqual(TEXT("同图停止恢复宿主provider"),S->GetReadiness().RegisteredRegionProviders,1);
   if(!Begin(Regions))return Finish();Stage=4;return false;
  }
  if(Stage==4&&V.State==EDivineBeastsDemoRunState::Running){
   ForeignOwner=World->SpawnActor<AActor>();FGamePlatformRegionProvider P;P.Provider=ForeignOwner;P.ContextGeneration=Generation;
   P.DefinitionId=FPrimaryAssetId(TEXT("GamePlatformDefinition:world.demo.region@1"));FGamePlatformId::TryParse(TEXT("world.demo.region@1"),P.RegionId);P.Bounds=FBox(FVector(-2000,-2000,-500),FVector(2000,2000,500));
   FGamePlatformResult R;Foreign=S->RegisterRegionProvider(P,R);if(!Test->TestTrue(TEXT("真实外部provider登记"),R.IsSuccess()&&Foreign.IsValid()))return Finish();Stop();Stage=5;return false;
  }
  if(Stage==5&&!Demo->HasActiveScenario()){
   auto* Host=World->GetSubsystem<UDivineBeastsDemoManualWorldHostSubsystem>();
   Test->TestEqual(TEXT("冲突明确保留DuplicateRegionId"),Host->GetLastResult().Code,FName(TEXT("DuplicateRegionId")));
   Test->TestEqual(TEXT("外部provider未被接管/撤销"),S->GetReadiness().RegisteredRegionProviders,1);
   Test->TestTrue(TEXT("仅外部拥有者撤销自身句柄"),S->UnregisterRegionProvider(Foreign));Foreign={};ForeignOwner->Destroy();ForeignOwner.Reset();
   if(!Begin(PCG))return Finish();Stage=6;return false;
  }
  if(Stage==6&&V.State==EDivineBeastsDemoRunState::Running){
   Test->TestTrue(TEXT("PCG页恢复默认provider就绪"),S->GetReadiness().bRequiredRegionsRegistered);
   Test->TestTrue(TEXT("World初始化代次不随页重建"),S->GetReadiness().Context.ContextGeneration==Generation);Stop();Stage=7;return false;
  }
  if(Stage==7&&!Demo->HasActiveScenario()){Test->TestTrue(TEXT("真实Run通知已覆盖交接"),Notifications>=8);return Finish();}
  return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualWorldHandoffTest,"DivineBeasts.Demo.WorldLive.ManualHostProviderHandoff",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FManualWorldHandoffTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FManualHandoffSequence(*this));return true;}

namespace {
class FManualHostReentrySequence final:public IAutomationLatentCommand {
 FAutomationTestBase* Test;TWeakObjectPtr<UDivineBeastsDemoWorldSubsystem> Demo;TWeakObjectPtr<UWorld> World;
 FDivineBeastsDemoScenarioDefinition Successor;FDelegateHandle Handler;FGuid Generation;
 double Deadline=FPlatformTime::Seconds()+45;bool bStarted=false,bTriggered=false,bStopping=false;
public:
 explicit FManualHostReentrySequence(FAutomationTestBase& T):Test(&T){}
 ~FManualHostReentrySequence(){if(Demo.IsValid())Demo->OnRunChanged.Remove(Handler);}
 bool Update()override{
  if(FPlatformTime::Seconds()>Deadline){Test->AddError(TEXT("重入后继原生运行超时"));if(Demo.IsValid())Demo->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("测试超时")));return true;}
  if(!bStarted){
   if(GEngine)for(const auto& C:GEngine->GetWorldContexts())if(C.World()&&C.World()->GetSubsystem<UDivineBeastsDemoManualWorldHostSubsystem>())World=C.World();
   if(!World.IsValid()){Test->AddError(TEXT("缺真实ManualWorldHost"));return true;}
   Demo=World->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();auto* C=World->GetGameInstance()->GetSubsystem<UDivineBeastsDemoCatalogSubsystem>();FDivineBeastsDemoScenarioDefinition First;
   if(!Demo.IsValid()||!C||Demo->HasActiveScenario()||!C->FindScenarioDefinition(TEXT("GamePlatformWorld.regions.Manual"),First)||!C->FindScenarioDefinition(TEXT("GamePlatformPCG.inspection.Manual"),Successor)){Test->AddError(TEXT("需要空闲真实Catalog/执行器"));return true;}
   auto S=IGamePlatformWorldService::Get(*World)->GetReadiness();if(S.Context.ReadinessState!=EGamePlatformWorldReadiness::Ready)return false;Generation=S.Context.ContextGeneration;
   Handler=Demo->OnRunChanged.AddLambda([this](const FDivineBeastsDemoRunSnapshot& V){
    if(bTriggered||V.FeatureId!=TEXT("GamePlatformWorld.regions")||V.State!=EDivineBeastsDemoRunState::Preparing)return;
    bTriggered=true;Test->TestTrue(TEXT("Preparing真实通知中Stop"),Demo->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("同步重入测试"))));
    Test->TestFalse(TEXT("停止完成释放旧运行"),Demo->HasActiveScenario());
    Test->TestTrue(TEXT("同一事件栈接受后继"),Demo->BeginScenario(Successor));
   });
   bStarted=true;Test->TestTrue(TEXT("旧Region Begin受理"),Demo->BeginScenario(First));return false;
  }
  if(!Demo.IsValid()||!World.IsValid()){Test->AddError(TEXT("测试世界丢失"));return true;}
  const auto V=Demo->GetRunSnapshot();
  if(V.State==EDivineBeastsDemoRunState::Failed){Test->AddError(TEXT("后继真实准备失败：")+V.OperationResult.Code.ToString());Demo->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("测试收敛")));return true;}
  if(!bStopping&&V.State==EDivineBeastsDemoRunState::Running){
   Test->TestTrue(TEXT("Preparing事件已真实重入"),bTriggered);Test->TestEqual(TEXT("旧回调未覆盖后继Feature"),V.FeatureId,FName(TEXT("GamePlatformPCG.inspection")));
   const auto S=IGamePlatformWorldService::Get(*World)->GetReadiness();Test->TestTrue(TEXT("后继保留同World初始化与默认provider"),S.Context.ContextGeneration==Generation&&S.bRequiredRegionsRegistered);
   bStopping=true;Demo->RequestScenarioStop(FGamePlatformResult::Cancelled(TEXT("后继测试完成")));return false;
  }
  return bStopping&&!Demo->HasActiveScenario();
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FManualWorldReentryTest,"DivineBeasts.Demo.WorldLive.ManualHostPreparingStopAndSuccessor",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FManualWorldReentryTest::RunTest(const FString&){ADD_LATENT_AUTOMATION_COMMAND(FManualHostReentrySequence(*this));return true;}
#endif

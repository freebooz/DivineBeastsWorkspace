#include "Scenarios/DivineBeastsDemoManualWorldHostSubsystem.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Interfaces/IGamePlatformWorldService.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Async/Async.h"

bool UDivineBeastsDemoManualWorldHostSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
 const auto* W=Cast<UWorld>(Outer);const auto* Args=FCommandLine::Get();
 return Super::ShouldCreateSubsystem(Outer)&&!UE_BUILD_SHIPPING&&!UE_BUILD_TEST&&!IsRunningCommandlet()&&
 FDivineBeastsPluginDemoMode::IsHubPageEnabled()&&FParse::Param(Args,TEXT("GamePlatformManualFixture"))&&
 FParse::Param(Args,TEXT("FoundationWorld"))&&FParse::Param(Args,TEXT("DBAManualWorldHost"))&&
 W&&W->WorldType==EWorldType::Game&&W->GetNetMode()==NM_Standalone&&
 W->GetOutermost()->GetName()==FDivineBeastsPluginDemoMode::GetHubMapPackage();
}
void UDivineBeastsDemoManualWorldHostSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
 Super::Initialize(Collection);Collection.InitializeDependency<UDivineBeastsDemoWorldSubsystem>();
}
void UDivineBeastsDemoManualWorldHostSubsystem::OnWorldBeginPlay(UWorld& W)
{
 Super::OnWorldBeginPlay(W);
 if(bAttempted||bClosing||&W!=GetWorld())return;
 bAttempted=true;
 if(!ShouldCreateSubsystem(&W)||!W.GetAuthGameMode()){
  LastResult=FGamePlatformResult::Failure(TEXT("Demo.WorldHost.Scope"),TEXT("只允许明确选择的Hub Standalone Authority世界"));return;
 }
 auto* S=IGamePlatformWorldService::Get(W);
 if(!S){LastResult=FGamePlatformResult::Failure(TEXT("Demo.WorldHost.Service"),TEXT("真实World服务缺失"));return;}
 const auto Initial=S->GetReadiness();WorldGeneration=Initial.Context.ContextGeneration;
 if(Initial.Context.ReadinessState!=EGamePlatformWorldReadiness::Uninitialized){
  LastResult=FGamePlatformResult::Failure(TEXT("Demo.WorldHost.ExternalContext"),TEXT("世界已经初始化，拒绝覆盖外部上下文"));return;
 }
 StateSubscription=S->SubscribeReadiness(this,[Weak=TWeakObjectPtr<ThisClass>(this)](const auto& Value){
  auto* Self=Weak.Get();if(!Self||Self->bClosing||Value.Context.ContextGeneration!=Self->WorldGeneration)return;
  if(Value.Context.ReadinessState==EGamePlatformWorldReadiness::Invalidated){Self->Close();return;}
  // World发布期间不写其登记表；单次有界任务，既无Timer也无循环重试。
  Self->QueueReconcile();
 },LastResult);
 if(!LastResult.IsSuccess()||!StateSubscription.IsValid())return;
 Demo=W.GetSubsystem<UDivineBeastsDemoWorldSubsystem>();
 if(!Demo.IsValid()){LastResult=FGamePlatformResult::Failure(TEXT("Demo.WorldHost.DemoMissing"),TEXT("唯一Demo世界执行器缺失"));S->UnsubscribeReadiness(StateSubscription);StateSubscription={};return;}
 RunSubscription=Demo->OnRunChanged.AddUObject(this,&ThisClass::RunChanged);
 FGamePlatformVersion V;V.Minor=1;
 LastResult=S->InitializeDevelopment(FPrimaryAssetId(TEXT("GamePlatformDefinition:world.demo.hub@1")),V);
 bInitialized=LastResult.IsSuccess();
 if(bInitialized){RunChanged(Demo->GetRunSnapshot());Reconcile();}
 // 没有Reset API；初始化租约由唯一World服务持有到World退出。
}
void UDivineBeastsDemoManualWorldHostSubsystem::RunChanged(const FDivineBeastsDemoRunSnapshot& V)
{
 if(bClosing||!bInitialized||!Demo.IsValid())return;
 const auto Current=Demo->GetRunSnapshot();
 if(!(Current.Token==V.Token)||Current.State!=V.State)return; // 同步重入后旧广播副本不得改写后继意图。
 const bool Regions=V.FeatureId==TEXT("GamePlatformWorld.regions")&&
 (V.State==EDivineBeastsDemoRunState::Preparing||V.State==EDivineBeastsDemoRunState::Running||V.State==EDivineBeastsDemoRunState::Stopping||V.bCleanupPending);
 if(Regions==bRegionPage)return;
 bRegionPage=Regions;++Epoch;bQueued=false;
 Reconcile(true); // Preparing在Adapter.Prepare之前发布，正常交接同步完成。
}
void UDivineBeastsDemoManualWorldHostSubsystem::QueueReconcile()
{
 if(bQueued||bClosing)return;bQueued=true;
 const uint64 E=Epoch;const FGuid G=WorldGeneration;
 AsyncTask(ENamedThreads::GameThread,[Weak=TWeakObjectPtr<ThisClass>(this),E,G]{
  auto* Self=Weak.Get();if(!Self||Self->bClosing||Self->Epoch!=E||Self->WorldGeneration!=G)return;
  Self->bQueued=false;Self->Reconcile();
 });
}
void UDivineBeastsDemoManualWorldHostSubsystem::Reconcile(bool bAllowDeferred)
{
 if(bClosing||!bInitialized||bReconciling||!GetWorld()||GetWorld()->bIsTearingDown)return;
 TGuardValue<bool> Guard(bReconciling,true);auto* S=IGamePlatformWorldService::Get(*GetWorld());if(!S)return;
 const auto V=S->GetReadiness();if(bClosing||V.Context.ContextGeneration!=WorldGeneration)return;
 if(V.Context.ReadinessState==EGamePlatformWorldReadiness::Failed){LastResult=V.Context.Result;return;}
 if(bRegionPage){
  if(!Provider.IsValid()){LastResult=FGamePlatformResult::Success();return;}
  if(S->UnregisterRegionProvider(Provider)){Provider={};LastResult=FGamePlatformResult::Success();}
  else {LastResult=FGamePlatformResult::Failure(TEXT("Demo.WorldHost.HandoffPending"),TEXT("自有Provider撤销被拒绝；保留完整句柄；异步交接最多一次，失败需显式准备重试"));if(bAllowDeferred)QueueReconcile();}
  return;
 }
 if(Provider.IsValid()){LastResult=FGamePlatformResult::Success();return;}
 if(!V.bRegionDefinitionsLoaded){LastResult=FGamePlatformResult();return;}
 FGamePlatformRegionProvider P;P.Provider=this;P.ContextGeneration=WorldGeneration;
 P.DefinitionId=FPrimaryAssetId(TEXT("GamePlatformDefinition:world.demo.region@1"));FGamePlatformId::TryParse(TEXT("world.demo.region@1"),P.RegionId);
 P.Bounds=FBox(FVector(-2000,-2000,-500),FVector(2000,2000,500));
 Provider=S->RegisterRegionProvider(P,LastResult); // DuplicateRegionId真实失败，绝不撤外部/Run provider。
}
FGamePlatformResult UDivineBeastsDemoManualWorldHostSubsystem::PrepareRun(const FDivineBeastsDemoRunToken& Token,FName Feature)
{
 if(bClosing||!bInitialized||!Demo.IsValid())return FGamePlatformResult::Failure(TEXT("Demo.WorldHost.Uninitialized"),TEXT("专用世界未成功初始化"));
 const auto V=Demo->GetRunSnapshot();
 if(!(V.Token==Token)||V.FeatureId!=Feature||V.State!=EDivineBeastsDemoRunState::Preparing)
  return FGamePlatformResult::Failure(TEXT("Demo.WorldHost.RunMismatch"),TEXT("仅真实当前Preparing运行可以请求区域交接"));
 RunChanged(V);Reconcile();
 // 已受理World初始化但定义仍在加载时允许绑定只读页面，随后真实通知更新；
 // 页面准备成功不代表World Ready，具体命令仍由原服务校验，冲突/失败照实拒绝。
 return LastResult.Status==EGamePlatformResultStatus::NotExecuted?FGamePlatformResult::Success():LastResult;
}
void UDivineBeastsDemoManualWorldHostSubsystem::Close()
{
 if(bClosing)return;bClosing=true;++Epoch;bQueued=false;
 if(Demo.IsValid())Demo->OnRunChanged.Remove(RunSubscription);RunSubscription.Reset();Demo.Reset();
 if(GetWorld())if(auto* S=IGamePlatformWorldService::Get(*GetWorld())){
  S->UnsubscribeReadiness(StateSubscription);StateSubscription={};
  // EndPlay中World服务可能已关闭：其自身会撤销所有Provider，绝不释放/重置世界初始化租约。
  if(Provider.IsValid()&&S->GetReadiness().Context.ReadinessState!=EGamePlatformWorldReadiness::Invalidated)
   if(!S->UnregisterRegionProvider(Provider))LastResult=FGamePlatformResult::Failure(TEXT("Demo.WorldHost.ExitProvider"),TEXT("等待World所有者退出清理本Provider"));
 }
 Provider={};bInitialized=false;
}
void UDivineBeastsDemoManualWorldHostSubsystem::OnWorldEndPlay(UWorld& W){Close();Super::OnWorldEndPlay(W);}
void UDivineBeastsDemoManualWorldHostSubsystem::Deinitialize(){Close();Super::Deinitialize();}

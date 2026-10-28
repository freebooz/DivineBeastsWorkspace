#include "Scenarios/DivineBeastsDemoWorldScenarios.h"
#include "Scenarios/DivineBeastsDemoPCGAuthoringPolicy.h"
#include "Types/GamePlatformWaterTypes.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Subsystems/GamePlatformWeatherWorldSubsystem.h"
#include "Actors/GamePlatformWeatherReplicator.h"
#include "Subsystems/GamePlatformWaterClientWorldSubsystem.h"
#include "Subsystems/GamePlatformTelemetrySubsystem.h"
#include "Subsystems/GamePlatformWeatherClientWorldSubsystem.h"
#include "Interfaces/IGamePlatformSurfaceService.h"
#if WITH_EDITOR
#include "Subsystems/GamePlatformNavigationWorldSubsystem.h"
#include "Data/GamePlatformNavigationAgentProfile.h"
#endif
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#include "Scenarios/DivineBeastsDemoWorldState.h"
#include "Tests/DivineBeastsDemoPresentationReentryFixture.h"
#include "Components/GamePlatformInteractableComponent.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "PCGComponent.h"
#include "Components/BoxComponent.h"
#include "PCGGraph.h"
#include "Sinks/GamePlatformTelemetrySink.h"
#include "Engine/LocalPlayer.h"
#include "Blueprint/UserWidget.h"
#include "EngineUtils.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#if WITH_EDITOR
#include "Links/GamePlatformNavigationSmartLinkProxy.h"
#include "Authoring/GamePlatformPCGManualAuthoring.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#endif
#include <limits>
#if WITH_DEV_AUTOMATION_TESTS
struct FGamePlatformTelemetryDevelopmentTestAccess {
 static TSharedPtr<IGamePlatformTelemetrySink,ESPMode::ThreadSafe> SinkOf(UGamePlatformTelemetrySubsystem& S){FScopeLock Lock(&S.SinkMutex);return S.Sink;}
 static FString SeedOf(const UGamePlatformTelemetrySubsystem& S){return S.SamplingSeed;}
 static bool EnabledOf(const UGamePlatformTelemetrySubsystem& S){return S.bEnabled;}
 /** 测试只在同一引擎拥有的GI上交换空闲槽；不Start/Flush/Shutdown原对象，不改变上下文或策略。 */
 static bool ExchangeIdleSink(UGamePlatformTelemetrySubsystem& S,const TSharedPtr<IGamePlatformTelemetrySink,ESPMode::ThreadSafe>& Expected,TSharedPtr<IGamePlatformTelemetrySink,ESPMode::ThreadSafe> Replacement){
  if(S.bClosing||!S.bInitialized||S.bConfiguringSink||S.bBeginningDevelopmentCapture||S.bEndingDevelopmentCapture||S.bFlushInProgress||S.FlushTickerHandle.IsValid()||S.DevelopmentCaptureScope.IsValid())return false;
  TGuardValue<bool> Inspecting(S.bBeginningDevelopmentCapture,true);const auto D=S.GetDiagnostics();
  if(D.BufferDepth!=0||D.PendingNetworkBatches!=0||S.bClosing||!S.bInitialized||S.bFlushInProgress||S.FlushTickerHandle.IsValid())return false;
  FScopeLock Lock(&S.SinkMutex);if(S.Sink!=Expected)return false;S.Sink=MoveTemp(Replacement);++S.SinkGeneration;return true;
 }
};
using namespace DivineBeasts::Demo;
namespace {
FDivineBeastsDemoScenarioDefinition ReadinessContract(){FDivineBeastsDemoScenarioDefinition D;D.FeatureId=TEXT("GamePlatformWorld.readiness");D.ScenarioId=TEXT("GamePlatformWorld.readiness.Manual");D.AdapterId=TEXT("PlatformWorld.GamePlatformWorld.readiness");D.Operations=WorldManualOperations(D.FeatureId);FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("ContributorResult");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Integer;P.Minimum=0;P.Maximum=2;D.Parameters.Add(P);return D;}
FDivineBeastsDemoScenarioDefinition RegionContract(){auto D=ReadinessContract();D.FeatureId=TEXT("GamePlatformWorld.regions");D.ScenarioId=TEXT("GamePlatformWorld.regions.Manual");D.AdapterId=TEXT("PlatformWorld.GamePlatformWorld.regions");D.Operations=WorldManualOperations(D.FeatureId);D.Parameters.Reset();for(FName N:{FName(TEXT("RegionId")),FName(TEXT("DefinitionId")),FName(TEXT("DefinitionVersion")),FName(TEXT("X")),FName(TEXT("Y"))}){FDivineBeastsDemoParameterDefinition P;P.ParameterId=N;P.DefaultValue.Type=N==TEXT("DefinitionVersion")?EDivineBeastsDemoParameterType::Integer:(N==TEXT("X")||N==TEXT("Y"))?EDivineBeastsDemoParameterType::Number:EDivineBeastsDemoParameterType::Identifier;P.Minimum=N==TEXT("DefinitionVersion")?1:-10000;P.Maximum=N==TEXT("DefinitionVersion")?65535:10000;D.Parameters.Add(P);}return D;}
TArray<FDivineBeastsDemoParameterValue> RegionValues(){auto D=RegionContract();TArray<FDivineBeastsDemoParameterValue> V;for(const auto& P:D.Parameters){auto X=P.DefaultValue;X.IdentifierValue=TEXT("world.demo.region");X.IntegerValue=1;V.Add(X);}return V;}
UWorld* LiveManualWorld(){if(!GEngine)return nullptr;for(const auto& C:GEngine->GetWorldContexts())if(auto* W=C.World())if(W->WorldType==EWorldType::Game&&W->GetNetMode()==NM_Standalone&&W->HasBegunPlay()&&W->GetGameInstance())return W;return nullptr;}
int32 WorldRequestCount(const FGamePlatformDataDiagnostics& D){return D.PendingRequests+D.ActiveLeases+D.TerminalRequests;}
bool SameWorldLease(const FGamePlatformDataLease& A,const FGamePlatformDataLease& B){return A.ScopeId==B.ScopeId&&A.LeaseId==B.LeaseId&&A.Generation==B.Generation&&A.IssuerProof==B.IssuerProof&&A.DefinitionId==B.DefinitionId&&A.Bundles==B.Bundles&&A.ResourcePaths==B.ResourcePaths;}
struct FWorldNativeFixture {
 FAutomationTestBase* Test=nullptr;UWorld* World=nullptr;TStrongObjectPtr<UDivineBeastsDemoPresentationTestScreen> Widget;
 TUniquePtr<IDivineBeastsDemoScenarioAdapter> Adapter;TSharedPtr<FWorldManualState> State;FDivineBeastsDemoScenarioContext Context;FManualValues Values;
 bool Create(FAutomationTestBase& T,FName Feature){
  Test=&T;World=LiveManualWorld();if(!FParse::Param(FCommandLine::Get(),TEXT("WorldManualNative"))||!World||!World->GetGameInstance()->GetFirstGamePlayer()){T.AddError(TEXT("HostRequired: WorldLive -game Hub, WorldManualNative, DBAPluginDemo, GamePlatformManualFixture"));return false;}
  auto* PC=World->GetGameInstance()->GetFirstGamePlayer()->GetPlayerController(World);Widget.Reset(CreateWidget<UDivineBeastsDemoPresentationTestScreen>(PC));if(!Widget.IsValid()){T.AddError(TEXT("真实LP测试Widget不能创建。"));return false;}
  Context.World=World;Context.SampleWidget=Widget.Get();Context.Token.RunId=FGuid::NewGuid();Context.Token.WorldId=FGuid::NewGuid();Context.Token.Generation=1;
  Context.Scenario.FeatureId=Feature;Context.Scenario.AdapterId=FName(*(TEXT("PlatformWorld.")+Feature.ToString()));Context.Scenario.ScenarioId=FName(*(Feature.ToString()+TEXT(".Manual")));Context.Scenario.Operations=WorldManualOperations(Feature);
  auto Add=[&](FName N,FDivineBeastsDemoParameterValue V){FDivineBeastsDemoParameterDefinition P;P.ParameterId=N;P.DefaultValue=V;P.Minimum=-10000;P.Maximum=2147483647;Context.Scenario.Parameters.Add(P);Values.Add(V);};
  auto Num=[&](FName N,double V){FDivineBeastsDemoParameterValue X;X.Type=EDivineBeastsDemoParameterType::Number;X.NumberValue=V;Add(N,X);};
  auto Int=[&](FName N,int64 V){FDivineBeastsDemoParameterValue X;X.Type=EDivineBeastsDemoParameterType::Integer;X.IntegerValue=V;Add(N,X);};
  auto Bool=[&](FName N,bool V){FDivineBeastsDemoParameterValue X;X.Type=EDivineBeastsDemoParameterType::Boolean;X.bBooleanValue=V;Add(N,X);};
  auto Id=[&](FName N,FName V){FDivineBeastsDemoParameterValue X;X.Type=EDivineBeastsDemoParameterType::Identifier;X.IdentifierValue=V;Add(N,X);};
  if(Feature==TEXT("GamePlatformNavigation.links")){Num(TEXT("Distance"),300);Bool(TEXT("Enabled"),true);Id(TEXT("TraversalType"),TEXT("Walk"));}
  else if(Feature==TEXT("GamePlatformInteraction.focus")){Num(TEXT("Distance"),150);Bool(TEXT("Pointer"),false);Bool(TEXT("Enabled"),true);}
  else if(Feature==TEXT("GamePlatformInteraction.session")){Num(TEXT("Distance"),150);Num(TEXT("HoldSeconds"),2);Bool(TEXT("LineOfSight"),true);}
  else if(Feature==TEXT("GamePlatformInteraction.outcome")){Id(TEXT("OptionId"),TEXT("Manual.Toggle"));Int(TEXT("CommitKind"),1);Int(TEXT("Charges"),0);}
  else if(Feature==TEXT("GamePlatformPCG.authoring")){
   FDivineBeastsDemoParameterValue P;P.Type=EDivineBeastsDemoParameterType::AssetPath;P.AssetPathValue=FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualPCG.DA_DBA_ManualPCG"));Add(TEXT("Profile"),P);
   P.AssetPathValue=FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualMeshSet.DA_DBA_ManualMeshSet"));Add(TEXT("MeshSet"),P);Id(TEXT("OutputName"),TEXT("PCG_DBA_ManualNative"));
  }
  else if(Feature==TEXT("GamePlatformPCG.inspection")){FDivineBeastsDemoParameterValue X;X.Type=EDivineBeastsDemoParameterType::AssetPath;X.AssetPathValue=FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualPCGRuntime.DA_DBA_ManualPCGRuntime"));Add(TEXT("Profile"),X);Int(TEXT("Seed"),1);}
  else {T.AddError(TEXT("未声明本测试夹具Feature。"));return false;}
  Adapter=CreateWorldManualScenarioForTesting(*World,Feature,State);if(!Adapter||!State.IsValid())return false;
  bool Done=false;FGamePlatformResult R;Adapter->Prepare(Context,[&](const auto& Token,const auto& Result){T.TestTrue(TEXT("Prepare返回完整Token"),Token==Context.Token);Done=true;R=Result;});
  return T.TestTrue(TEXT("真实适配器已准备（不绕过Prepare）"),Done&&R.IsSuccess());
 }
 void SetProfile(const TCHAR* P){Values[0].AssetPathValue=FSoftObjectPath(P);}
 FGamePlatformResult Stop(){FGamePlatformResult R;bool Done=false;if(Adapter)Adapter->Stop([&](const auto& T,const auto& V){Test->TestTrue(TEXT("Stop返回同一完整Token"),T==Context.Token);Done=true;R=V;});if(!Done)Test->AddError(TEXT("Stop没有终态回调。"));return R;}
 ~FWorldNativeFixture(){if(Adapter)Stop();}
};
struct FWorldLiveSequence:TSharedFromThis<FWorldLiveSequence> {
 FAutomationTestBase* Test=nullptr;TSharedPtr<FWorldNativeFixture> Fixture;TArray<TFunction<void()>> Steps;int32 Step=0;bool Ready=true,Finished=false;double Deadline=0;FGamePlatformResult Last;
 IGamePlatformDataService* Data=nullptr;int32 Baseline=0,ObjectsBefore=0;int64 AcceptedRequests=0;FGamePlatformDataLease ForeignProfile,ForeignResources;int32 Applies=0;
 TWeakObjectPtr<UBoxComponent> OldBounds;TWeakObjectPtr<UPCGComponent> OldComponent;TStrongObjectPtr<AActor> RetainedAnchor;
 void Signal(const FDivineBeastsDemoRunToken& T,const FGamePlatformResult& R){Test->TestTrue(TEXT("操作返回原完整Token"),T==Fixture->Context.Token);Last=R;Ready=true;}
 void Issue(FName O){Ready=false;const TWeakPtr<FWorldLiveSequence> Weak=AsShared();Fixture->Adapter->Execute(O,Fixture->Values,[Weak](const auto& T,const auto& R){if(auto S=Weak.Pin())S->Signal(T,R);});}
 void Load(const FSoftObjectPath& Path){Ready=false;const TWeakPtr<FWorldLiveSequence> Weak=AsShared();Fixture->State->Load(Path,[Weak](UObject* O){auto S=Weak.Pin();if(!S)return FGamePlatformResult::Cancelled(TEXT("测试作用域失效。"));++S->Applies;S->Test->TestTrue(TEXT("Apply只在本运行完整定义束成功后执行"),S->Fixture->State->DefinitionLease.IsValid()&&S->Data->GetLeaseState(S->Fixture->State->DefinitionLease)==EGamePlatformDataRequestState::Succeeded&&S->Fixture->State->DefinitionLease.Bundles.Contains(TEXT("PCGGeneration")));return FGamePlatformResult::Success();},[Weak](const auto& T,const auto& R){if(auto S=Weak.Pin())S->Signal(T,R);});}
 bool Init(FAutomationTestBase& T,FName Feature){Test=&T;Fixture=MakeShared<FWorldNativeFixture>();if(!Fixture->Create(T,Feature))return false;Data=Fixture->State->Data();if(!Data)return false;const auto D=Data->GetDiagnostics();if(D.PendingRequests){T.AddError(TEXT("需要无外部在途Data工作的独占测试窗口，不初始化替代Data。"));return false;}Baseline=WorldRequestCount(D);Deadline=FPlatformTime::Seconds()+90;return true;}
 void AcquireForeign(const TArray<FSoftObjectPath>& Paths,bool bProfile){
  Ready=false;const TWeakPtr<FWorldLiveSequence> Weak=AsShared();FGamePlatformResult Accepted;
  auto H=Data->AcquireResources(Paths,EGamePlatformDataLifetime::World,Fixture->World,[Weak,bProfile](const auto& H,const auto& R){auto S=Weak.Pin();if(!S)return;const auto& Own=bProfile?S->ForeignProfile:S->ForeignResources;S->Test->TestTrue(TEXT("外部租约完整身份匹配"),SameWorldLease(Own,H));S->Last=R;S->Ready=true;},Accepted);
  if(bProfile)ForeignProfile=H;else ForeignResources=H;if(!Accepted.IsSuccess()){Test->AddError(TEXT("真实外部资源租约未被接纳，需先制作原生夹具。"));Finish();}
 }
 void Finish(){if(Finished)return;if(Fixture&&Fixture->Adapter)Test->TestTrue(TEXT("真实Stop清理成功"),Fixture->Stop().IsSuccess());
  if(Data){if(ForeignProfile.IsValid()){Test->TestTrue(TEXT("本运行清理保留外部Profile租约"),Data->GetLeaseState(ForeignProfile)==EGamePlatformDataRequestState::Succeeded);Test->TestTrue(TEXT("只释放测试自己的外部Profile租约"),Data->ReleaseResources(ForeignProfile).IsSuccess());ForeignProfile={};}if(ForeignResources.IsValid()){Test->TestTrue(TEXT("本运行清理保留外部资源租约"),Data->GetLeaseState(ForeignResources)==EGamePlatformDataRequestState::Succeeded);Test->TestTrue(TEXT("只释放测试自己的外部图/网格租约"),Data->ReleaseResources(ForeignResources).IsSuccess());ForeignResources={};}Test->TestEqual(TEXT("Pending/Active/Terminal合计回到外部基线"),WorldRequestCount(Data->GetDiagnostics()),Baseline);}Finished=true;Steps.Reset();}
};
class FWorldLiveSequenceWait final:public IAutomationLatentCommand {
 TSharedRef<FWorldLiveSequence> Run;
public:explicit FWorldLiveSequenceWait(TSharedRef<FWorldLiveSequence> R):Run(R){}
 bool Update()override{if(Run->Finished)return true;if(FPlatformTime::Seconds()>Run->Deadline){Run->Test->AddError(TEXT("World真实操作/资源终态超时，未视为通过。"));Run->Finish();return true;}if(Run->Ready){if(Run->Step==Run->Steps.Num()){Run->Finish();return true;}Run->Ready=false;auto Action=MoveTemp(Run->Steps[Run->Step++]);Action();}return Run->Finished;}
};
class FGamePlatformTelemetryInstrumentedSink final:public IGamePlatformTelemetrySink {
public:int32 Starts=0,Flushes=0,Shutdowns=0,Submits=0;FGamePlatformTelemetrySinkStatus Status;mutable TFunction<void()> OnHealth;
 bool Start()override{++Starts;Status.Health=EGamePlatformTelemetrySinkHealth::Healthy;return true;}
 void SubmitBatch(FGamePlatformTelemetryBatch,FGamePlatformTelemetrySubmitCompletion Done)override{++Submits;if(Done)Done(true,false);}
 void Flush()override{++Flushes;}
 void Shutdown(float)override{++Shutdowns;Status.Health=EGamePlatformTelemetrySinkHealth::Stopped;}
 FGamePlatformTelemetrySinkStatus GetHealth()const override{auto Hook=MoveTemp(OnHealth);if(Hook)Hook();return Status;}
};
bool RunTelemetryInstrumentedCapture(FAutomationTestBase& T){
 auto* W=LiveManualWorld();if(!W){T.AddError(TEXT("HostRequired: WorldLive Standalone真实GI"));return false;}auto* S=W->GetGameInstance()->GetSubsystem<UGamePlatformTelemetrySubsystem>();if(!S)return false;
 const auto Precondition=S->GetContextSnapshot();if(!Precondition.SessionId.IsEmpty()||!Precondition.PseudonymousPlayerId.IsEmpty()||!Precondition.MapId.IsEmpty()||!Precondition.WorldId.IsEmpty()||!Precondition.ExperienceId.IsEmpty()||!Precondition.MatchId.IsEmpty()||!Precondition.ArenaModeId.IsEmpty()||!Precondition.CorrelationId.IsEmpty()||!Precondition.TransactionId.IsEmpty()||!Precondition.ServerInstanceId.IsEmpty()){T.AddError(TEXT("HostRequired: 独占空合成上下文；不接管真实用户/世界数据。"));return false;}
 const auto RootOriginal=FGamePlatformTelemetryDevelopmentTestAccess::SinkOf(*S);const bool OuterEnabled=FGamePlatformTelemetryDevelopmentTestAccess::EnabledOf(*S);const FString OuterSeed=FGamePlatformTelemetryDevelopmentTestAccess::SeedOf(*S);TSharedPtr<FGamePlatformTelemetryInstrumentedSink,ESPMode::ThreadSafe> Probe=MakeShared<FGamePlatformTelemetryInstrumentedSink,ESPMode::ThreadSafe>();Probe->Start();
 if(!FGamePlatformTelemetryDevelopmentTestAccess::ExchangeIdleSink(*S,RootOriginal,Probe)){T.AddError(TEXT("测试只能观察/交换原来空闲的真实GI槽，不能清掉旧工作来满足前置。"));Probe->Shutdown(0);return false;}
 FGuid H;ON_SCOPE_EXIT {Probe->OnHealth={};if(H.IsValid())S->EndDevelopmentCapture(H);auto Current=FGamePlatformTelemetryDevelopmentTestAccess::SinkOf(*S);if(Current==Probe||!Current.IsValid()){S->SetEnabled(OuterEnabled);S->SetSamplingSeed(OuterSeed);T.TestTrue(TEXT("测试结束精确归还外层原sink，不Configure/Flush/Shutdown它"),FGamePlatformTelemetryDevelopmentTestAccess::ExchangeIdleSink(*S,Current,RootOriginal));}else T.AddError(TEXT("测试出现未知后继sink，拒绝覆盖。"));Probe->Shutdown(0);};
 Probe->Status.PendingBatches=1;Probe->Status.PendingCapacity=2;T.TestFalse(TEXT("有真实sink在途批次拒绝capture"),S->BeginDevelopmentCapture(*W,FGuid::NewGuid(),H));T.TestFalse(TEXT("拒绝不签发句柄"),H.IsValid());T.TestTrue(TEXT("拒绝保留精确原Probe"),FGamePlatformTelemetryDevelopmentTestAccess::SinkOf(*S)==Probe);Probe->Status.PendingBatches=0;
 S->SetEnabled(true);FGamePlatformTelemetryEvent Event;Event.EventName=TEXT("Telemetry.Foundation.WorldLoaded");Event.SchemaVersion=1;T.TestEqual(TEXT("同一真实Recorder接纳有界合成事件"),S->RecordEvent(Event),EGamePlatformTelemetryRecordResult::Recorded);
 T.TestTrue(TEXT("实际缓冲非空"),S->GetDiagnostics().BufferDepth>0);T.TestFalse(TEXT("有缓冲/已安排flush拒绝capture"),S->BeginDevelopmentCapture(*W,FGuid::NewGuid(),H));T.TestTrue(TEXT("缓冲拒绝没有替换sink"),FGamePlatformTelemetryDevelopmentTestAccess::SinkOf(*S)==Probe);S->FlushBestEffort();T.TestTrue(TEXT("合成批次通过真实Recorder提交给Probe"),Probe->Submits>0);T.TestEqual(TEXT("真实flush后空缓冲"),S->GetDiagnostics().BufferDepth,0);
 S->SetEnabled(false);const auto BeforeContext=S->GetContextSnapshot();const FString Seed=FGamePlatformTelemetryDevelopmentTestAccess::SeedOf(*S);const bool Enabled=FGamePlatformTelemetryDevelopmentTestAccess::EnabledOf(*S);const auto Before=S->GetDiagnostics();TSharedPtr<FGamePlatformTelemetryInstrumentedSink,ESPMode::ThreadSafe> Other=MakeShared<FGamePlatformTelemetryInstrumentedSink,ESPMode::ThreadSafe>();bool Reentered=false;
 Probe->OnHealth=[&]{Reentered=true;S->SetEnabled(!Enabled);S->SetSamplingSeed(TEXT("manual.blocked"));S->BeginSession(TEXT("manual.blocked"),TEXT("manual.blocked"));T.TestFalse(TEXT("旧GetHealth重入Configure被拒绝"),S->ConfigureSink(Other));};
 T.TestTrue(TEXT("真实GetHealth边界可安全capture"),S->BeginDevelopmentCapture(*W,FGuid::NewGuid(),H));T.TestTrue(TEXT("GetHealth重入确实执行"),Reentered);T.TestEqual(TEXT("重入候选没有Start"),Other->Starts,0);if(!H.IsValid())return false;
 T.TestFalse(TEXT("运行时槽为独立合成计数sink"),FGamePlatformTelemetryDevelopmentTestAccess::SinkOf(*S)==Probe);T.TestEqual(TEXT("暂停原sink不Flush"),Probe->Flushes,0);T.TestEqual(TEXT("暂停原sink不Shutdown"),Probe->Shutdowns,0);
 EGamePlatformTelemetryRecordResult Recorded;T.TestTrue(TEXT("scope合成事件实际受理"),S->ExecuteDevelopmentCapture(H,TEXT("RecordEvent"),1,TEXT("native"),Recorded));T.TestEqual(TEXT("真实记录枚举"),Recorded,EGamePlatformTelemetryRecordResult::Recorded);S->ExecuteDevelopmentCapture(H,TEXT("Flush"),0,TEXT("native"),Recorded);
 T.TestTrue(TEXT("精确End恢复"),S->EndDevelopmentCapture(H));H.Invalidate();T.TestTrue(TEXT("恢复的是同一个原sink对象"),FGamePlatformTelemetryDevelopmentTestAccess::SinkOf(*S)==Probe);T.TestEqual(TEXT("恢复不重新Start原对象"),Probe->Starts,1);T.TestEqual(TEXT("恢复不Flush原对象"),Probe->Flushes,0);T.TestEqual(TEXT("恢复不Shutdown原对象"),Probe->Shutdowns,0);T.TestTrue(TEXT("原采样种子精确恢复且不输出内容"),FGamePlatformTelemetryDevelopmentTestAccess::SeedOf(*S)==Seed);T.TestEqual(TEXT("原Enabled恢复"),FGamePlatformTelemetryDevelopmentTestAccess::EnabledOf(*S),Enabled);T.TestEqual(TEXT("原上下文会话值恢复"),S->GetContextSnapshot().SessionId,BeforeContext.SessionId);T.TestEqual(TEXT("原buffer计数对象恢复"),S->GetDiagnostics().RecordedTotal,Before.RecordedTotal);
 T.TestTrue(TEXT("测试只把空闲槽设为真正空指针"),FGamePlatformTelemetryDevelopmentTestAccess::ExchangeIdleSink(*S,Probe,{}));T.TestTrue(TEXT("真正空sink仍能capture"),S->BeginDevelopmentCapture(*W,FGuid::NewGuid(),H));if(!H.IsValid())return false;T.TestTrue(TEXT("空sink精确恢复"),S->EndDevelopmentCapture(H));H.Invalidate();T.TestFalse(TEXT("恢复仍为空，不用新NullSink冒充"),FGamePlatformTelemetryDevelopmentTestAccess::SinkOf(*S).IsValid());T.TestTrue(TEXT("恢复测试Probe供作用域退出归还外层对象"),FGamePlatformTelemetryDevelopmentTestAccess::ExchangeIdleSink(*S,{},Probe));return true;
}
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFeaturesTest,"DivineBeasts.PluginDemos.World.FeatureWhitelist",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldFeaturesTest::RunTest(const FString&){auto F=WorldManualFeatures();TSet<FName> Unique;for(FName X:F)Unique.Add(X);TestEqual(TEXT("26 stable candidate features"),F.Num(),26);TestEqual(TEXT("no duplicate feature identities"),Unique.Num(),26);TestTrue(TEXT("all features have explicit stop"),!F.ContainsByPredicate([](FName X){return !WorldManualOperations(X).Contains(TEXT("Stop"));}));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldContractTest,"DivineBeasts.PluginDemos.World.ContractRejectsInjectedOperations",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldContractTest::RunTest(const FString&){auto D=ReadinessContract();TestTrue(TEXT("real exact contract accepted"),ValidateWorldManualContract(D));D.Operations.Add(TEXT("Exec"));TestFalse(TEXT("script-like operation refused"),ValidateWorldManualContract(D));D=ReadinessContract();D.AdapterId=TEXT("PlatformPresentation.GamePlatformWorld.readiness");TestFalse(TEXT("cross domain adapter refused"),ValidateWorldManualContract(D));return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldIdentifierTest,"DivineBeasts.PluginDemos.World.IdentifierAndAssetTypes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldIdentifierTest::RunTest(const FString&){auto D=RegionContract();auto V=RegionValues();TestTrue(TEXT("bounded identifier base accepted"),ValidateWorldManualValues(D,V).IsSuccess());for(const TCHAR* X:{TEXT("world.demo@1"),TEXT("Type:Value"),TEXT("/Game/Foo"),TEXT("bad id")}){V[0].IdentifierValue=FName(X);TestFalse(TEXT("logical version/path cannot leak into identifier"),ValidateWorldManualValues(D,V).IsSuccess());}V=RegionValues();V[0].Type=EDivineBeastsDemoParameterType::AssetPath;TestFalse(TEXT("wrong typed value refused"),ValidateWorldManualValues(D,V).IsSuccess());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldNumberTest,"DivineBeasts.PluginDemos.World.NonFiniteAndRange",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldNumberTest::RunTest(const FString&){auto D=RegionContract();auto V=RegionValues();V[3].NumberValue=std::numeric_limits<double>::quiet_NaN();TestFalse(TEXT("NaN refused"),ValidateWorldManualValues(D,V).IsSuccess());V[3].NumberValue=10001;TestFalse(TEXT("range refused"),ValidateWorldManualValues(D,V).IsSuccess());V[3].NumberValue=1;V[2].IntegerValue=0;TestFalse(TEXT("version must be positive"),ValidateWorldManualValues(D,V).IsSuccess());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldTokenTest,"DivineBeasts.PluginDemos.World.FullRunIdentity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldTokenTest::RunTest(const FString&){FDivineBeastsDemoRunToken A;A.RunId=FGuid::NewGuid();A.WorldId=FGuid::NewGuid();A.Generation=1;auto B=A;B.WorldId=FGuid::NewGuid();TestFalse(TEXT("same RunId different World refused"),A==B);B=A;++B.Generation;TestFalse(TEXT("same GUIDs stale epoch refused"),A==B);return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldRotatedWaterTest,"DivineBeasts.PluginDemos.World.RotatedWaterAndClearedMiss",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldRotatedWaterTest::RunTest(const FString&){FGamePlatformWaterRegionConfig C;C.RegionId=TEXT("Manual.Water");C.ShallowDepthCm=60;C.FlowSpeedCmPerSecond=20;FGamePlatformWaterSample S;FTransform T(FRotator(0,90,0),FVector::ZeroVector);TestTrue(TEXT("rotated real water math"),FGamePlatformWaterMath::SampleWaterVolume(T,FVector(300,100,200),100,FVector(0,200,0),C,S));TestEqual(TEXT("actual depth"),S.DepthCm,100.f);TestFalse(TEXT("deep classification"),S.bShallow);TestFalse(TEXT("rotated outside miss"),FGamePlatformWaterMath::SampleWaterVolume(T,FVector(300,100,200),100,FVector(200,0,0),C,S));TestFalse(TEXT("miss clears stale sample"),S.bInWater);TestTrue(TEXT("miss clears stale region"),S.RegionId.IsNone());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldFactoryTest,"DivineBeasts.PluginDemos.World.FactoryRejectsForeignFeature",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldFactoryTest::RunTest(const FString&){TStrongObjectPtr<UWorld> W(NewObject<UWorld>());TestFalse(TEXT("unknown feature has no fallback executor"),CreateWorldManualScenario(*W,TEXT("GamePlatformSFX.spaces")).IsValid());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldSourceTest,"DivineBeasts.PluginDemos.World.WrongWorldThenStop",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldSourceTest::RunTest(const FString&){TStrongObjectPtr<UWorld> A(NewObject<UWorld>()),B(NewObject<UWorld>());auto Adapter=CreateWorldManualScenario(*A,TEXT("GamePlatformWorld.readiness"));FDivineBeastsDemoScenarioContext C;C.Token.RunId=FGuid::NewGuid();C.Token.WorldId=FGuid::NewGuid();C.Token.Generation=1;C.World=B.Get();C.Scenario=ReadinessContract();int32 Done=0;Adapter->Prepare(C,[&](const auto& T,const auto& R){++Done;TestTrue(TEXT("original token returned"),T==C.Token);TestFalse(TEXT("wrong world rejected"),R.IsSuccess());});Adapter->Stop([&](const auto&,const auto& R){++Done;TestTrue(TEXT("failed preparation has no foreign ownership"),R.IsSuccess());});TArray<FDivineBeastsDemoParameterValue> Values;Adapter->Execute(TEXT("Snapshot"),Values,[&](const auto&,const auto& R){++Done;TestFalse(TEXT("stopped executor cannot revive"),R.IsSuccess());});TestEqual(TEXT("one callback for each command"),Done,3);return true;}
// Mandatory standalone gate: these use the engine-created live GI/World collection and never Init/Deinit a second service.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldTelemetryScopeTest,"DivineBeasts.PluginDemos.World.Standalone.TelemetryExactRestore",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldTelemetryScopeTest::RunTest(const FString&){return RunTelemetryInstrumentedCapture(*this);}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldWeatherScopeTest,"DivineBeasts.PluginDemos.World.Standalone.WeatherOwnedCleanup",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldWeatherScopeTest::RunTest(const FString&){auto* W=LiveManualWorld();if(!W){AddError(TEXT("HostRequired: standalone explicit manual fixture"));return false;}auto* S=W->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();if(!S)return false;FGuid H;TestTrue(TEXT("acquire unactivated world"),S->BeginDevelopmentWeather(FGuid::NewGuid(),H));if(!H.IsValid())return false;TestFalse(TEXT("foreign ordinary write cannot take over scope"),S->SetWeather({},0));TestTrue(TEXT("same authoritative service activated"),S->DevelopmentWeather(H,TEXT("Activate"),{}, {},0,0));TestTrue(TEXT("real revision published"),S->GetCurrentSnapshot().Revision>0);TestFalse(TEXT("foreign cleanup rejected"),S->EndDevelopmentWeather(FGuid::NewGuid()));TestTrue(TEXT("own actor/plan cleanup"),S->EndDevelopmentWeather(H));TestEqual(TEXT("real snapshot returned unactivated"),S->GetCurrentSnapshot().Revision,0);return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldWeatherSpawnReentryTest,"DivineBeasts.PluginDemos.World.Standalone.WeatherSpawnStopReentry",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldWeatherSpawnReentryTest::RunTest(const FString&){auto* W=LiveManualWorld();if(!W){AddError(TEXT("HostRequired: standalone explicit manual fixture"));return false;}auto* S=W->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();if(!S)return false;FGuid Scope;if(!S->BeginDevelopmentWeather(FGuid::NewGuid(),Scope)){AddError(TEXT("pristine weather scope required"));return false;}bool Reentered=false;auto Event=W->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([&](AActor* A){if(A&&A->IsA<AGamePlatformWeatherReplicator>()){Reentered=true;TestTrue(TEXT("scope can stop inside native actor spawn"),S->EndDevelopmentWeather(Scope));}}));const bool Activated=S->DevelopmentWeather(Scope,TEXT("Activate"),{}, {},0,0);W->RemoveOnActorSpawnedHandler(Event);TestTrue(TEXT("real actor-spawn boundary exercised"),Reentered);TestFalse(TEXT("old spawn stack cannot report success"),Activated);TestEqual(TEXT("old actor cannot republish weather"),S->GetCurrentSnapshot().Revision,0);return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldWaterScopeTest,"DivineBeasts.PluginDemos.World.Standalone.WaterQualityRestore",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldWaterScopeTest::RunTest(const FString&){auto* W=LiveManualWorld();if(!W){AddError(TEXT("HostRequired: standalone explicit manual fixture"));return false;}auto* S=W->GetSubsystem<UGamePlatformWaterClientWorldSubsystem>();if(!S)return false;auto Q=S->GetWaterQualityTier();FGuid H;TestTrue(TEXT("acquire pristine presentation"),S->BeginDevelopmentPresentation(FGuid::NewGuid(),H));if(!H.IsValid())return false;TestFalse(TEXT("foreign quality write rejected"),S->SetWaterQualityTier(EGamePlatformWaterQualityTier::Low));TestTrue(TEXT("own quality change"),S->SetDevelopmentWaterQuality(H,EGamePlatformWaterQualityTier::Low));TestFalse(TEXT("foreign cleanup rejected"),S->EndDevelopmentPresentation(FGuid::NewGuid()));TestTrue(TEXT("exact quality restoration"),S->EndDevelopmentPresentation(H));TestEqual(TEXT("saved quality restored"),S->GetWaterQualityTier(),Q);return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldWeatherProjectionTest,"DivineBeasts.PluginDemos.World.Standalone.WeatherPreservesSuccessorProjection",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldWeatherProjectionTest::RunTest(const FString&){auto* W=LiveManualWorld();if(!W){AddError(TEXT("HostRequired: standalone explicit manual fixture"));return false;}auto* Runtime=W->GetSubsystem<UGamePlatformWeatherWorldSubsystem>();auto* Client=W->GetSubsystem<UGamePlatformWeatherClientWorldSubsystem>();auto* Surface=IGamePlatformSurfaceService::Get(*W);if(!Runtime||!Client||!Surface)return false;const auto Baseline=Surface->GetEnvironmentState();FGuid Projection,Weather;if(!Client->BeginDevelopmentProjection(FGuid::NewGuid(),Projection)){AddError(TEXT("pristine zero-weather projection required"));return false;}if(!Runtime->BeginDevelopmentWeather(FGuid::NewGuid(),Weather)){Client->EndDevelopmentProjection(Projection);return false;}TestTrue(TEXT("real weather activated"),Runtime->DevelopmentWeather(Weather,TEXT("Activate"),{}, {},0,0));auto Foreign=Surface->GetEnvironmentState();Foreign.GlobalWetness=.123f;TestTrue(TEXT("independent producer accepted by real Surface"),Surface->ApplyEnvironmentState(Foreign).IsStateAccepted());FGamePlatformWeatherState Cloudy;Cloudy.Type=EGamePlatformWeatherType::Cloudy;Runtime->DevelopmentWeather(Weather,TEXT("SetWeather"),Cloudy,{},0,0);TestEqual(TEXT("later weather does not overwrite successor wetness"),Surface->GetEnvironmentState().GlobalWetness,.123f);TestTrue(TEXT("own runtime cleanup"),Runtime->EndDevelopmentWeather(Weather));TestTrue(TEXT("own projection relinquished"),Client->EndDevelopmentProjection(Projection));TestEqual(TEXT("cleanup preserves successor wetness"),Surface->GetEnvironmentState().GlobalWetness,.123f);Surface->ApplyEnvironmentState(Baseline);return true;}
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldNavProfileTest,"DivineBeasts.PluginDemos.World.Standalone.NavigationProfileOwnership",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldNavProfileTest::RunTest(const FString&){auto* W=LiveManualWorld();if(!W){AddError(TEXT("HostRequired: Editor -game standalone fixture"));return false;}auto* S=W->GetSubsystem<UGamePlatformNavigationWorldSubsystem>();if(!S){AddError(TEXT("Editor-only NavigationServer host required"));return false;}TStrongObjectPtr<UGamePlatformNavigationAgentProfile> A(NewObject<UGamePlatformNavigationAgentProfile>()),Foreign(NewObject<UGamePlatformNavigationAgentProfile>());A->ProfileId=FName(*(TEXT("Manual.Native.")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));Foreign->ProfileId=A->ProfileId;TestTrue(TEXT("real profile registered"),S->RegisterAgentProfile(A.Get()));TestFalse(TEXT("duplicate cannot replace owner"),S->RegisterAgentProfile(Foreign.Get()));TestFalse(TEXT("foreign same-id pointer cannot withdraw owner"),S->UnregisterAgentProfile(Foreign.Get()));TestTrue(TEXT("exact owner can withdraw"),S->UnregisterAgentProfile(A.Get()));TestTrue(TEXT("new owner may register after exact withdrawal"),S->RegisterAgentProfile(Foreign.Get()));TestTrue(TEXT("new owner cleanup"),S->UnregisterAgentProfile(Foreign.Get()));return true;}
#endif
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldInteractionDefaultsTest,"DivineBeasts.PluginDemos.World.InteractionArgumentDefaults",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldInteractionDefaultsTest::RunTest(const FString&){FDivineBeastsDemoScenarioContext C;FDivineBeastsDemoParameterDefinition P;P.ParameterId=TEXT("Distance");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Number;C.Scenario.Parameters.Add(P);FDivineBeastsDemoParameterValue V=P.DefaultValue;V.NumberValue=150;auto A=DecodeWorldInteractionFixture(FManualArguments(C,{V}));TestEqual(TEXT("未声明Charges的真实解码保持可用默认3"),A.Charges,3);P.ParameterId=TEXT("Charges");P.DefaultValue.Type=EDivineBeastsDemoParameterType::Integer;C.Scenario.Parameters.Add(P);auto Zero=P.DefaultValue;Zero.IntegerValue=0;A=DecodeWorldInteractionFixture(FManualArguments(C,{V,Zero}));TestEqual(TEXT("显式Outcome零保留为负向输入"),A.Charges,0);return true;}
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldLinkSpawnStopTest,"DivineBeasts.PluginDemos.World.Standalone.LinkSpawnStopReentry",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldLinkSpawnStopTest::RunTest(const FString&){
 FWorldNativeFixture Old;if(!Old.Create(*this,TEXT("GamePlatformNavigation.links")))return false;auto* W=Old.World;int32 Before=0;for(TActorIterator<AGamePlatformNavigationSmartLinkProxy> I(W);I;++I)++Before;
 bool Spawned=false;TWeakObjectPtr<AGamePlatformNavigationSmartLinkProxy> Candidate;FDivineBeastsDemoRunToken Next=Old.Context.Token;Next.RunId=FGuid::NewGuid();++Next.Generation;
 auto H=W->AddOnActorSpawnedHandler(FOnActorSpawned::FDelegate::CreateLambda([&](AActor* A){if(auto* L=Cast<AGamePlatformNavigationSmartLinkProxy>(A)){Spawned=true;Candidate=L;const auto Stop=Old.Stop();TestFalse(TEXT("出生栈未收敛时Stop不得提前成功"),Stop.IsSuccess());TestEqual(TEXT("明确保留出生所有权重试"),Stop.Code,FName(TEXT("Demo.Nav.SpawnInFlight")));TestTrue(TEXT("未收敛时旧宿主仍被持有"),Old.State->Host->Owns(Old.Context.Token));TestFalse(TEXT("不能在旧候选未收敛时接管后继宿主"),Old.State->Host->Acquire(Next,*W).IsSuccess());}}));
 FGamePlatformResult Result;bool Done=false;Old.Adapter->Execute(TEXT("CreateLink"),Old.Values,[&](const auto& T,const auto& R){TestTrue(TEXT("CreateLink返回原Token"),T==Old.Context.Token);Result=R;Done=true;});W->RemoveOnActorSpawnedHandler(H);
 TestTrue(TEXT("真实SpawnActor回调已覆盖"),Spawned);TestTrue(TEXT("旧操作回调一次"),Done);TestEqual(TEXT("旧命令取消而非成功"),Result.Status,EGamePlatformResultStatus::Cancelled);TestFalse(TEXT("本候选没有存活"),Candidate.IsValid());TestFalse(TEXT("旧状态不发布Link"),Old.State->Link.IsValid());TestTrue(TEXT("候选收敛后Stop重试成功"),Old.Stop().IsSuccess());
 FWorldNativeFixture Successor;if(!Successor.Create(*this,TEXT("GamePlatformNavigation.links")))return false;Successor.Adapter->Execute(TEXT("CreateLink"),Successor.Values,[&](const auto&,const auto& R){TestTrue(TEXT("后继真实Link创建成功"),R.IsSuccess());});const auto SuccessorLink=Successor.State->Link;TestTrue(TEXT("后继Link有效"),SuccessorLink.IsValid());TestTrue(TEXT("旧运行重复Stop幂等"),Old.Stop().IsSuccess());TestTrue(TEXT("旧Stop不销毁后继Link"),SuccessorLink.IsValid()&&Successor.State->Link==SuccessorLink);TestTrue(TEXT("旧Stop不释放后继完整宿主Token"),Successor.State->Host->Owns(Successor.Context.Token));TestTrue(TEXT("后继自行精确清理"),Successor.Stop().IsSuccess());int32 After=0;for(TActorIterator<AGamePlatformNavigationSmartLinkProxy> I(W);I;++I)++After;TestEqual(TEXT("真实Link数量回到外部基线"),After,Before);return true;
}
#endif
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldInteractionTargetDefaultsTest,"DivineBeasts.PluginDemos.World.Standalone.InteractionTargetDefaults",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldInteractionTargetDefaultsTest::RunTest(const FString&){
 auto R=MakeShared<FWorldLiveSequence>();if(!R->Init(*this,TEXT("GamePlatformInteraction.focus")))return false;
 auto Count=[R](){int32 N=0;for(TActorIterator<AActor> I(R->Fixture->World);I;++I)if(I->FindComponentByClass<UGamePlatformInteractableComponent>())++N;return N;};R->ObjectsBefore=Count();
 R->Steps.Add([R]{R->AcquireForeign({FSoftObjectPath(TEXT("/Engine/BasicShapes/Sphere.Sphere"))},false);});
 for(FName Feature:{FName(TEXT("GamePlatformInteraction.focus")),FName(TEXT("GamePlatformInteraction.session"))}){
  R->Steps.Add([R,Feature]{R->Test->TestTrue(TEXT("外部可见网格租约成功且保留"),R->Data->GetLeaseState(R->ForeignResources)==EGamePlatformDataRequestState::Succeeded);if(R->Fixture->Context.Scenario.FeatureId!=Feature){R->Fixture=MakeShared<FWorldNativeFixture>();if(!R->Fixture->Create(*R->Test,Feature)){R->Finish();return;}}R->Issue(TEXT("CreateTarget"));});
  R->Steps.Add([R,Count]{auto S=R->Fixture->State;R->Test->TestTrue(TEXT("focus/session真实CreateTarget正常成功"),R->Last.IsSuccess());R->Test->TestTrue(TEXT("可用目标和唯一选项"),S->Target.IsValid()&&S->Interactable.IsValid()&&S->Interactable->IsInteractionEnabled()&&S->Interactable->GetOptions().Num()==1&&S->Interactable->IsOptionAvailable(S->Interactable->GetOptions()[0]));if(S->Interactable.IsValid())R->Test->TestEqual(TEXT("缺省Charges保持3"),S->Interactable->GetRemainingCharges(),3);R->Test->TestEqual(TEXT("只新增一个真实目标"),Count(),R->ObjectsBefore+1);R->Issue(TEXT("CreateTarget"));});
  R->Steps.Add([R,Count]{R->Test->TestFalse(TEXT("重复创建明确拒绝而非第二目标"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("重复后仍唯一目标"),Count(),R->ObjectsBefore+1);R->Issue(TEXT("Reset"));});
  R->Steps.Add([R,Count]{R->Test->TestTrue(TEXT("Reset成功"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("Reset没有遗留目标"),Count(),R->ObjectsBefore);R->Test->TestEqual(TEXT("本资源释放保留外部租约基线"),WorldRequestCount(R->Data->GetDiagnostics()),R->Baseline+1);R->Issue(TEXT("CreateTarget"));});
  R->Steps.Add([R,Count]{R->Test->TestTrue(TEXT("Reset后真实重建成功"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("重建仍仅一个目标"),Count(),R->ObjectsBefore+1);R->Test->TestTrue(TEXT("本场景Stop成功"),R->Fixture->Stop().IsSuccess());R->Test->TestEqual(TEXT("Stop恢复目标基线"),Count(),R->ObjectsBefore);R->Ready=true;});
 }
 R->Steps.Add([R]{R->Fixture=MakeShared<FWorldNativeFixture>();if(!R->Fixture->Create(*R->Test,TEXT("GamePlatformInteraction.outcome"))){R->Finish();return;}R->Issue(TEXT("CreateTarget"));});
 R->Steps.Add([R,Count]{R->Test->TestFalse(TEXT("Outcome显式Charges0是独立负向"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("零Charges准确错误码"),R->Last.Code,FName(TEXT("Demo.Interaction.Charges")));R->Test->TestFalse(TEXT("负向不遗留目标"),R->Fixture->State->Target.IsValid());R->Test->TestEqual(TEXT("负向仍为原目标基线"),Count(),R->ObjectsBefore);R->Ready=true;});
 ADD_LATENT_AUTOMATION_COMMAND(FWorldLiveSequenceWait(R));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldDefinitionDependencyRetryTest,"DivineBeasts.PluginDemos.World.Standalone.DefinitionDependencyRetry",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldDefinitionDependencyRetryTest::RunTest(const FString&){
 auto R=MakeShared<FWorldLiveSequence>();if(!R->Init(*this,TEXT("GamePlatformPCG.inspection")))return false;
 const FSoftObjectPath Broken(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualPCGMissingDependency.DA_DBA_ManualPCGMissingDependency"));
 R->Steps.Add([R,Broken]{R->AcquireForeign({Broken},true);});
 R->Steps.Add([R,Broken]{if(!R->Last.IsSuccess()){R->Test->AddError(TEXT("HostRequired: 制作真实MissingDependency Profile fixture。"));R->Finish();return;}auto* P=Cast<UGamePlatformPCGProfileDefinition>(Broken.ResolveObject());if(!P||P->GraphReference.IsNull()||P->OutputMesh.IsNull()){R->Test->AddError(TEXT("缺依赖Profile必须仍有真实Graph/Mesh软引用。"));R->Finish();return;}R->AcquireForeign({P->GraphReference.ToSoftObjectPath(),P->OutputMesh.ToSoftObjectPath()},false);});
 R->Steps.Add([R,Broken]{if(!R->Last.IsSuccess()){R->Test->AddError(TEXT("真实外部图/网格不能加载。"));R->Finish();return;}auto* P=Cast<UGamePlatformPCGProfileDefinition>(Broken.ResolveObject());R->Test->TestTrue(TEXT("外部租约使真实图/网格常驻"),P&&P->GraphReference.Get()&&P->OutputMesh.Get());R->Load(Broken);});
 R->Steps.Add([R,Broken]{R->Test->TestFalse(TEXT("缺递归依赖首次真实失败"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("首次失败未调用Apply"),R->Applies,0);R->Test->TestTrue(TEXT("普通对象租约成功与定义失败独立"),R->Data->GetLeaseState(R->Fixture->State->AssetLease)==EGamePlatformDataRequestState::Succeeded&&R->Data->GetLeaseState(R->Fixture->State->DefinitionLease)!=EGamePlatformDataRequestState::Succeeded);R->Load(Broken);});
 R->Steps.Add([R]{R->Test->TestFalse(TEXT("同路径重试不能借外部常驻资源绕过定义"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("同路径重试仍未Apply"),R->Applies,0);R->Test->TestEqual(TEXT("重试具体阻塞原因"),R->Last.Code,FName(TEXT("Demo.World.DefinitionNotReady")));R->Test->TestTrue(TEXT("Reset只释放本运行失败/成功租约"),R->Fixture->State->ReleaseAsset().IsSuccess());R->Test->TestEqual(TEXT("保留两个外部记录，包含全部请求状态"),WorldRequestCount(R->Data->GetDiagnostics()),R->Baseline+2);R->Load(FSoftObjectPath(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualPCGRuntime.DA_DBA_ManualPCGRuntime")));});
 R->Steps.Add([R]{R->Test->TestTrue(TEXT("合法Profile取得本运行成功完整依赖束后才Apply"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("只对成功束Apply一次"),R->Applies,1);R->Ready=true;});
 ADD_LATENT_AUTOMATION_COMMAND(FWorldLiveSequenceWait(R));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldPCGComponentOwnershipTest,"DivineBeasts.PluginDemos.World.Standalone.PCGComponentOwnership",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldPCGComponentOwnershipTest::RunTest(const FString&){
 auto R=MakeShared<FWorldLiveSequence>();if(!R->Init(*this,TEXT("GamePlatformPCG.inspection")))return false;const auto Valid=R->Fixture->Values[0].AssetPathValue;
 auto Count=[R](){TArray<UPCGComponent*> C;if(R->Fixture->State->Anchor.IsValid())R->Fixture->State->Anchor->GetComponents<UPCGComponent>(C);return C.Num();};
 auto CheckBounds=[R](){
  auto* S=R->Fixture->State.Get();auto* B=S->PCGBounds.Get();auto* C=S->PCGComponent.Get();
  R->Test->TestTrue(TEXT("真实自有Box边界已登记"),B&&B->IsRegistered()&&B->GetOwner()==S->Anchor.Get());
  if(!B)return;
  R->OldBounds=B;
  auto* Profile=Cast<UGamePlatformPCGProfileDefinition>(R->Fixture->Values[0].AssetPathValue.ResolveObject());
  R->Test->TestTrue(TEXT("夹具体积来自本成功租约Profile"),Profile&&B->GetUnscaledBoxExtent().Equals(Profile->HalfExtentCm));
  const FBox Bounds=B->Bounds.GetBox();
  R->Test->TestTrue(TEXT("真实Primitive体积有效且三轴非零"),Bounds.IsValid&&Bounds.GetExtent().GetMin()>0);
  R->Test->TestTrue(TEXT("真实Box中心属于本Anchor位置"),Bounds.GetCenter().Equals(S->Anchor->GetActorLocation()));
  R->Test->TestTrue(TEXT("边界无碰撞重叠导航副作用"),B->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&!B->GetGenerateOverlapEvents()&&!B->CanEverAffectNavigation());
  R->Test->TestTrue(TEXT("PCG只按需生成且已登记"),C&&C->IsRegistered()&&C->GenerationTrigger==EPCGComponentGenerationTrigger::GenerateOnDemand);
  if(C){const FBox Native=C->GetExecutionState().GetBounds();R->Test->TestTrue(TEXT("原生执行源读真实Owner边界"),Native.IsValid&&Native.Min.Equals(Bounds.Min)&&Native.Max.Equals(Bounds.Max));R->Test->TestFalse(TEXT("配置不得隐式生成"),C->bGenerated||C->IsGenerating());}
 };
 R->Steps.Add([R,Valid]{R->AcquireForeign({Valid},true);});
 R->Steps.Add([R,Valid]{auto* P=Cast<UGamePlatformPCGProfileDefinition>(Valid.ResolveObject());if(!R->Last.IsSuccess()||!P){R->Test->AddError(TEXT("HostRequired: 真实Runtime PCG Profile"));R->Finish();return;}R->AcquireForeign({P->GraphReference.ToSoftObjectPath(),P->OutputMesh.ToSoftObjectPath()},false);});
 R->Steps.Add([R]{if(!R->Last.IsSuccess()){R->Test->AddError(TEXT("真实外部PCG图/网格租约失败。"));R->Finish();return;}R->Issue(TEXT("ConfigureComponent"));});
 R->Steps.Add([R,Count,CheckBounds]{R->Test->TestTrue(TEXT("真实Configure成功"),R->Last.IsSuccess());CheckBounds();R->Test->TestEqual(TEXT("第一次只有一个真实PCG组件"),Count(),1);R->OldComponent=R->Fixture->State->PCGComponent;R->Test->TestTrue(TEXT("本组件持有真实克隆图"),R->OldComponent.IsValid()&&R->OldComponent->GetGraph());R->AcceptedRequests=R->Data->GetDiagnostics().TotalAcceptedRequests;R->Issue(TEXT("ConfigureComponent"));});
 R->Steps.Add([R,Count]{R->Test->TestFalse(TEXT("第二次拒绝覆盖旧组件"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("重复拒绝具名错误码"),R->Last.Code,FName(TEXT("Demo.PCG.ComponentAlreadyOwned")));R->Test->TestTrue(TEXT("完整原组件Owner仍持有"),R->Fixture->State->PCGComponent==R->OldComponent);R->Test->TestEqual(TEXT("重复后真实组件数仍为1"),Count(),1);R->Test->TestEqual(TEXT("重复不制造额外Data请求"),R->Data->GetDiagnostics().TotalAcceptedRequests,R->AcceptedRequests);R->Issue(TEXT("Reset"));});
 R->Steps.Add([R,Count]{R->Test->TestTrue(TEXT("真实Reset成功"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("Reset后Anchor没有PCG组件"),Count(),0);R->Test->TestFalse(TEXT("旧组件不再存活"),R->OldComponent.IsValid());R->Test->TestFalse(TEXT("Reset移除真实自有边界"),R->OldBounds.IsValid()||R->Fixture->State->PCGBounds.IsValid());R->Test->TestEqual(TEXT("释放本资源且保留外部租约"),WorldRequestCount(R->Data->GetDiagnostics()),R->Baseline+2);R->Issue(TEXT("ConfigureComponent"));});
 R->Steps.Add([R,Count,CheckBounds]{R->Test->TestTrue(TEXT("重置后可重新配置"),R->Last.IsSuccess());CheckBounds();R->Test->TestEqual(TEXT("重新配置仍准确一个组件"),Count(),1);R->Issue(TEXT("Reset"));});
 R->Steps.Add([R]{R->Test->TestTrue(TEXT("切失败夹具前先Reset"),R->Last.IsSuccess());R->Fixture->SetProfile(TEXT("/DBAUIPack_Core/Development/PluginDemos/Fixtures/DA_DBA_ManualPCGInvalidGraph.DA_DBA_ManualPCGInvalidGraph"));R->Issue(TEXT("ConfigureComponent"));});
 R->Steps.Add([R,Count]{R->Test->TestFalse(TEXT("非法图真实配置失败"),R->Last.IsSuccess());R->Test->TestEqual(TEXT("真实非法图错误码"),R->Last.Code,FName(TEXT("UnsupportedGraphShape")));R->Test->TestEqual(TEXT("失败仍保留唯一组件Owner供Stop"),Count(),1);R->OldBounds=R->Fixture->State->PCGBounds;R->Test->TestTrue(TEXT("非法图不得登记PCG组件"),R->Fixture->State->PCGComponent.IsValid()&&!R->Fixture->State->PCGComponent->IsRegistered());R->OldComponent=R->Fixture->State->PCGComponent;R->RetainedAnchor.Reset(R->Fixture->State->Anchor.Get());R->Test->TestTrue(TEXT("失败后Stop准确清理"),R->Fixture->Stop().IsSuccess());TArray<UPCGComponent*> C;if(R->RetainedAnchor.Get())R->RetainedAnchor->GetComponents<UPCGComponent>(C);R->Test->TestEqual(TEXT("失败Stop不遗留组件/图Owner"),C.Num(),0);R->Test->TestFalse(TEXT("失败组件已销毁"),R->OldComponent.IsValid());R->Test->TestFalse(TEXT("失败Stop销毁本次真实Bounds"),R->OldBounds.IsValid()||R->Fixture->State->PCGBounds.IsValid());R->Test->TestEqual(TEXT("失败Stop仍保留外部全状态基线"),WorldRequestCount(R->Data->GetDiagnostics()),R->Baseline+2);R->Ready=true;});
 ADD_LATENT_AUTOMATION_COMMAND(FWorldLiveSequenceWait(R));return true;
}

#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldRealizedNativeTest,"DivineBeasts.Demo.WorldLive.RealizedAuthoringLeaseAndSave",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldRealizedNativeTest::RunTest(const FString&){
 auto Run=MakeShared<FWorldLiveSequence>();if(!Run->Init(*this,TEXT("GamePlatformPCG.authoring")))return false;
 const TWeakPtr<FWorldLiveSequence> Weak=Run;auto SourceGraph=MakeShared<FSoftObjectPath>();
 auto Saved=MakeShared<FString>();auto Name=MakeShared<FName>(*(TEXT("PCG_DBA_ManualNative")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
 Run->Steps.Add([Weak]{auto S=Weak.Pin();if(!S)return;FString Path;auto R=GamePlatformPCGManualAuthoring::CreateRealizedGraph(*S->Data,{},S->Fixture->Values[0].AssetPathValue,S->Fixture->Values[1].AssetPathValue,TEXT("PCG_DBA_ManualDenied"),[]{return true;},Path);S->Test->TestEqual(TEXT("驻留不能代替本Run定义租约"),R.Code,FName(TEXT("PCG.Manual.DefinitionLease")));S->Issue(TEXT("ValidateProfile"));});
 Run->Steps.Add([Weak,SourceGraph,Name]{auto S=Weak.Pin();if(!S)return;S->Test->TestTrue(TEXT("真实完整定义束加载成功；未Realized模板的图校验结果独立"),S->Data->GetLeaseState(S->Fixture->State->DefinitionLease)==EGamePlatformDataRequestState::Succeeded);auto* P=Cast<UGamePlatformPCGProfileDefinition>(S->Fixture->Values[0].AssetPathValue.ResolveObject());if(!P){S->Test->AddError(TEXT("原生Profile未加载"));S->Finish();return;}*SourceGraph=P->GraphReference.ToSoftObjectPath();S->Fixture->Values[2].IdentifierValue=*Name;S->Issue(TEXT("CreateRealizedGraph"));});
 Run->Steps.Add([Weak,Saved,SourceGraph]{auto S=Weak.Pin();if(!S)return;S->Test->TestTrue(TEXT("真实原作者创建并保存"),S->Last.IsSuccess());*Saved=S->Fixture->State->SavedRealizedGraph;S->Test->TestTrue(TEXT("持久图包真实存在"),FPackageName::DoesPackageExist(FSoftObjectPath(*Saved).GetLongPackageName()));auto* P=Cast<UGamePlatformPCGProfileDefinition>(S->Fixture->Values[0].AssetPathValue.ResolveObject());S->Test->TestTrue(TEXT("共享Profile图引用未改写"),P&&P->GraphReference.ToSoftObjectPath()==*SourceGraph);S->Issue(TEXT("CreateRealizedGraph"));});
 Run->Steps.Add([Weak,Saved]{auto S=Weak.Pin();if(!S)return;S->Test->TestEqual(TEXT("重复输出明确拒绝，不覆盖已有图"),S->Last.Code,FName(TEXT("PCG.Manual.OutputOccupied")));S->Test->TestTrue(TEXT("Stop释放完整租约"),S->Fixture->Stop().IsSuccess());S->Test->TestTrue(TEXT("Stop保留持久资产"),FPackageName::DoesPackageExist(FSoftObjectPath(*Saved).GetLongPackageName()));S->Ready=true;});
 ADD_LATENT_AUTOMATION_COMMAND(FWorldLiveSequenceWait(Run));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldRealizedCancelNativeTest,"DivineBeasts.Demo.WorldLive.RealizedAuthoringStopDuringAssetCreated",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldRealizedCancelNativeTest::RunTest(const FString&){
 auto Run=MakeShared<FWorldLiveSequence>();if(!Run->Init(*this,TEXT("GamePlatformPCG.authoring")))return false;
 const TWeakPtr<FWorldLiveSequence> Weak=Run;auto Subscription=MakeShared<FDelegateHandle>();auto Called=MakeShared<int32>(0);
 auto Name=MakeShared<FName>(*(TEXT("PCG_DBA_ManualCancel")+FGuid::NewGuid().ToString(EGuidFormats::Digits)));
 Run->Steps.Add([Weak]{if(auto S=Weak.Pin())S->Issue(TEXT("ValidateProfile"));});
 Run->Steps.Add([Weak,Subscription,Called,Name]{auto S=Weak.Pin();if(!S)return;
  *Subscription=FAssetRegistryModule::GetRegistry().OnAssetAdded().AddLambda([Weak,Called,Name](const FAssetData& Asset){
   if(Asset.AssetName!=*Name)return;if(auto R=Weak.Pin()){
    ++*Called;const auto H=R->Fixture->State->DefinitionLease;
    R->Test->TestFalse(TEXT("原生保存栈重入Stop不提前完成清理"),R->Fixture->Stop().IsSuccess());
    R->Test->TestTrue(TEXT("原作者仍持完整定义束"),SameWorldLease(H,R->Fixture->State->DefinitionLease)&&R->Data->GetLeaseState(H)==EGamePlatformDataRequestState::Succeeded);
   }
  });
  S->Fixture->Values[2].IdentifierValue=*Name;S->Issue(TEXT("CreateRealizedGraph"));
  FAssetRegistryModule::GetRegistry().OnAssetAdded().Remove(*Subscription);Subscription->Reset();
 });
 Run->Steps.Add([Weak,Called]{auto S=Weak.Pin();if(!S)return;
  S->Test->TestEqual(TEXT("真实AssetCreated回调恰一次"),*Called,1);
  S->Test->TestTrue(TEXT("旧操作最终取消"),S->Last.Status==EGamePlatformResultStatus::Cancelled);
  const FString Saved=S->Fixture->State->SavedRealizedGraph;
  S->Test->TestTrue(TEXT("已保存产物明确保留"),!Saved.IsEmpty()&&FPackageName::DoesPackageExist(FSoftObjectPath(Saved).GetLongPackageName()));
  S->Test->TestTrue(TEXT("同步保存返回后Stop重试收敛"),S->Fixture->Stop().IsSuccess());S->Ready=true;
 });
 ADD_LATENT_AUTOMATION_COMMAND(FWorldLiveSequenceWait(Run));return true;
}
#endif

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldRealizedProjectNameTest,"DivineBeasts.PluginDemos.World.RealizedProjectOutputScope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FWorldRealizedProjectNameTest::RunTest(const FString&){
 TestEqual(TEXT("裸前缀PCG_DBA_Manual必须拒绝空后缀"),ValidateManualRealizedName(TEXT("PCG_DBA_Manual")).Code,FName(TEXT("Demo.PCG.OutputScope")));
 TestTrue(TEXT("最短合法后缀PCG_DBA_ManualA放行"),ValidateManualRealizedName(TEXT("PCG_DBA_ManualA")).IsSuccess());
 TestTrue(TEXT("正式DBA名称放行"),ValidateManualRealizedName(TEXT("PCG_DBA_ManualSpatial")).IsSuccess());
 for(const TCHAR* Name:{TEXT(""),TEXT("None"),TEXT("PCG_ExampleScatter"),TEXT("pcg_dba_manualSpatial"),TEXT("PCG_DBA_Manual.WithDot"),TEXT("PCG_DBA_Manual../Outside")})
  TestEqual(FString::Printf(TEXT("项目提交边界拒绝输入[%s]"),Name),ValidateManualRealizedName(FName(Name)).Code,FName(TEXT("Demo.PCG.OutputScope")));
 FString BoundaryName=TEXT("PCG_DBA_Manual");while(BoundaryName.Len()<96)BoundaryName+=TEXT("A");
 TestTrue(TEXT("96字符合法名称放行"),ValidateManualRealizedName(FName(*BoundaryName)).IsSuccess());
 BoundaryName+=TEXT("A");
 TestEqual(TEXT("97字符名称拒绝"),ValidateManualRealizedName(FName(*BoundaryName)).Code,FName(TEXT("Demo.PCG.OutputScope")));
 return true;
}
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldRealizedProjectEarlyRejectTest,"DivineBeasts.Demo.WorldLive.RealizedProjectRejectBeforeData",EAutomationTestFlags::ClientContext|EAutomationTestFlags::EngineFilter)
bool FWorldRealizedProjectEarlyRejectTest::RunTest(const FString&){
 FWorldNativeFixture F;if(!F.Create(*this,TEXT("GamePlatformPCG.authoring")))return false;
 const auto Before=F.State->Data()->GetDiagnostics().TotalAcceptedRequests;
 for(const TCHAR* Name:{TEXT("PCG_ExampleScatter"),TEXT("pcg_dba_manualSpatial"),TEXT("PCG_DBA_Manual")}){
  F.Values[2].IdentifierValue=FName(Name);bool Completed=false;FGamePlatformResult Result;
  F.Adapter->Execute(TEXT("CreateRealizedGraph"),F.Values,[&](const auto&,const auto& R){Completed=true;Result=R;});
  TestTrue(FString::Printf(TEXT("输入[%s]在异步加载/平台作者之前同步拒绝"),Name),Completed);
  TestEqual(FString::Printf(TEXT("输入[%s]保留项目错误身份"),Name),Result.Code,FName(TEXT("Demo.PCG.OutputScope")));
 }
 TestEqual(TEXT("无Data请求进入平台"),F.State->Data()->GetDiagnostics().TotalAcceptedRequests,Before);
 TestTrue(TEXT("被拒绝后Stop可收敛"),F.Stop().IsSuccess());return true;
}
#endif
#endif

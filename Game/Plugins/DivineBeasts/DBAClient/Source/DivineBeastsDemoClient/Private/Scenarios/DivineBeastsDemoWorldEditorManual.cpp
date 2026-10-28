#include "Scenarios/DivineBeastsDemoWorldState.h"
#include "Scenarios/DivineBeastsDemoPCGAuthoringPolicy.h"
#if WITH_EDITOR
#include "Subsystems/GamePlatformNavigationWorldSubsystem.h"
#include "Data/GamePlatformNavigationAgentProfile.h"
#include "Links/GamePlatformNavigationSmartLinkProxy.h"
#include "NavLinkCustomComponent.h"
#include "Authoring/GamePlatformPCGManualAuthoring.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Definitions/GamePlatformPCGProfileDefinition.h"
#include "Actors/GamePlatformPCGActors.h"
#include "PCGGraph.h"
#include "Components/BoxComponent.h"
#include "Validation/GamePlatformValidationService.h"
#include "Review/GamePlatformReviewTypes.h"
#include "Editor.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/EngineVersion.h"
#endif
namespace DivineBeasts::Demo {
#if WITH_EDITOR
class FWorldManualRules final:public IGamePlatformValidationRuleProvider {
 FName Owner;
public:
 explicit FWorldManualRules(FName O):Owner(O){}
 FName GetProviderId()const override{return Owner;}
 void BuildRules(TArray<FGamePlatformValidationRule>& Out)const override{FGamePlatformValidationRule R;R.RuleId=Owner;R.Category=TEXT("ManualFixture");R.Description=TEXT("本Run规则元数据/门禁夹具；不表示真实资产审查已执行。尺度由正式规则决定。");R.Rationale=TEXT("验证扩展登记/撤销及GateStage判定。");R.Scope=TEXT("本开发Run");R.Owner=Owner.ToString();Out.Add(R);}
};
static FString WorldOutput(const FDivineBeastsDemoRunToken& Token){const FString P=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Validation/PluginManualUI20261027/WorldRuns")/Token.RunId.ToString(EGuidFormats::Digits));FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*P);return P;}
static FGamePlatformResult EditorResult(bool B,const FString& E){return B?FGamePlatformResult::Success():FGamePlatformResult::Failure(TEXT("Demo.World.EditorRejected"),E);}
FGamePlatformResult FWorldManualState::CreateOwnedSmartLink(double Distance,bool bEnabled,FName Traversal){
 if(bSpawningLink||Link.IsValid()||!RetiringLinks.IsEmpty())return WorldError(TEXT("Demo.Nav.LinkBusy"),TEXT("先核对并清理本运行旧Link/出生候选。"));
 const auto E=Epoch;auto KeepSelf=AsShared();TGuardValue<bool> Spawning(bSpawningLink,true);
 TStrongObjectPtr<AGamePlatformNavigationSmartLinkProxy> Candidate(World->SpawnActor<AGamePlatformNavigationSmartLinkProxy>());
 if(!Candidate.IsValid())return Current(E)?WorldError(TEXT("Demo.Nav.LinkSpawn"),TEXT("本SmartLink创建失败。")):FGamePlatformResult::Cancelled(TEXT("SmartLink出生期间已停止。"));
 // Stop在出生回调内明确保留宿主，直到本栈收敛这个尚未发布的候选。
 auto RetireCandidate=[&](){
  const auto Own=Candidate.Get();
  if(!IsValid(Own)||Own->IsActorBeingDestroyed()||Own->Destroy()){
   if(Link.Get()==Own)Link.Reset();
   return FGamePlatformResult::Cancelled(TEXT("旧SmartLink候选已精确退役；停止后不会发布或操作后继运行。"));
  }
  RetiringLinks.AddUnique(Own);
  return WorldError(TEXT("Demo.Nav.CandidateCleanupPending"),TEXT("本候选未确认销毁，保留精确Actor和宿主供Stop重试。"));
 };
 if(!Current(E))return RetireCandidate();
 Link=Candidate.Get();Candidate->SetFlags(RF_Transient);Candidate->NavigationLinkId=FName(*(TEXT("Manual.Link.")+Context.Token.RunId.ToString(EGuidFormats::Digits)));Candidate->TraversalType=Traversal;
 auto* Component=Candidate->GetSmartLinkComp();if(!Component)return RetireCandidate();
 Component->SetLinkData(FVector(-Distance*.5,0,0),FVector(Distance*.5,0,0),ENavLinkDirection::BothWays);
 if(!Current(E)||Link.Get()!=Candidate.Get())return RetireCandidate();
 Candidate->SetPlatformLinkEnabled(bEnabled);
 if(!Current(E)||Link.Get()!=Candidate.Get())return RetireCandidate();
 return FGamePlatformResult::Success();
}
#endif
FGamePlatformResult FWorldManualState::CleanupEditor(){
#if WITH_EDITOR
 const auto Retiring=RetiringLinks;
 for(const auto& H:Retiring){if(H.IsValid()&&!H->IsActorBeingDestroyed()&&!H->Destroy())return WorldError(TEXT("Demo.Nav.CandidateCleanupPending"),TEXT("本旧候选未确认销毁，保留句柄/宿主重试。"));RetiringLinks.RemoveAll([&](const auto& Own){return Own==H;});if(Link==H)Link.Reset();}
 if(World.IsValid())if(auto* N=World->GetSubsystem<UGamePlatformNavigationWorldSubsystem>()){
  if(PathRequest.IsValid()){const auto H=PathRequest;if(!N->CancelRequest(H)&&N->GetDiagnostics().OutstandingAsyncRequests>0)return WorldError(TEXT("Demo.Nav.Cancel"),TEXT("导航请求未确认取消，保留完整句柄。"));PathRequest={};}
  if(Invoker.IsValid()){N->UnregisterInvoker(*Invoker);if(!Invoker->Destroy())return WorldError(TEXT("Demo.Nav.InvokerCleanup"),TEXT("本Invoker未确认销毁。"));Invoker.Reset();}
  if(bOwnNavProfile){if(!NavProfile.IsValid()||!N->UnregisterAgentProfile(NavProfile.Get()))return WorldError(TEXT("Demo.Nav.ProfileCleanup"),TEXT("本Profile仍被使用或身份变更；保留租约供Stop重试。"));bOwnNavProfile=false;NavProfile.Reset();}
 }else if(PathRequest.IsValid()||Invoker.IsValid()||bOwnNavProfile)return WorldError(TEXT("Demo.Nav.CleanupHost"),TEXT("原Navigation服务失效，保留所有权。"));
 if(Link.IsValid()){if(!Link->Destroy())return WorldError(TEXT("Demo.Nav.LinkCleanup"),TEXT("本Link未确认销毁。"));Link.Reset();}
 if(Exclusion.IsValid()){if(bExclusionRegistered){if(!Director.IsValid()||!Director->UnregisterParticipant(Exclusion.Get()))return WorldError(TEXT("Demo.PCG.ParticipantCleanup"),TEXT("本参与者注册尚未确认撤销，保留Director/Actor/租约。"));bExclusionRegistered=false;}if(!Exclusion->Destroy())return WorldError(TEXT("Demo.PCG.ExclusionCleanup"),TEXT("本排除Actor未确认销毁。"));Exclusion.Reset();}
 if(Director.IsValid()){if(!Director->Destroy())return WorldError(TEXT("Demo.PCG.DirectorCleanup"),TEXT("本Director未确认销毁。"));Director.Reset();}SpatialGraph.Reset();
 if(!RuleProvider.IsNone()){FGamePlatformValidationService::UnregisterRuleProvider(RuleProvider);RuleProvider=NAME_None;}
 Review.Reset();Runner.Reset();bHasPerformance=false;
#endif
 return FGamePlatformResult::Success();
}
void FWorldManualState::EditorOperation(FName O,const FManualArguments& A,FDivineBeastsDemoOperationCompletion D){
#if !WITH_EDITOR
 D(Context.Token,WorldError(TEXT("Demo.World.EditorHostRequired"),TEXT("本功能需要Editor条件私有桥；普通Client不链接Editor或ServerOnly。")));
#else
 if(Feature.ToString().StartsWith(TEXT("GamePlatformNavigation."))){
  auto* N=World->GetSubsystem<UGamePlatformNavigationWorldSubsystem>();if(!N){D(Context.Token,WorldError(TEXT("Demo.Nav.HostRequired"),TEXT("Editor中的真实NavigationServer服务不可用；不手工初始化第二份服务。")));return;}
  if(O==TEXT("CreateLink")){
   const auto R=CreateOwnedSmartLink(A.Number(TEXT("Distance")),A.Bool(TEXT("Enabled")),A.Id(TEXT("TraversalType")));D(Context.Token,R);return;
  }
  if(O==TEXT("SetEnabled")||O==TEXT("InspectLink")){if(!Link.IsValid()){D(Context.Token,WorldError(TEXT("Demo.Nav.NoLink"),TEXT("先创建真实SmartLink。")));return;}if(O==TEXT("SetEnabled"))Link->SetPlatformLinkEnabled(A.Bool(TEXT("Enabled")));Observe(FString::Printf(TEXT("真实SmartLink Id=%s Enabled=%d；无真实AI路径遍历时不宣称Traversal通过。"),*Link->NavigationLinkId.ToString(),Link->IsSmartLinkEnabled()));D(Context.Token,FGamePlatformResult::Success());return;}
  if(O==TEXT("Reset")){D(Context.Token,CleanupEditor());return;}
  if(O==TEXT("CancelPath")){auto H=PathRequest;if(!H.IsValid()){D(Context.Token,WorldError(TEXT("Demo.Nav.NoPath"),TEXT("没有本Run异步请求。")));return;}bool B=N->CancelRequest(H);if(B)PathRequest={};D(Context.Token,WorldBool(B,TEXT("CancelRequest")));return;}
  if(O==TEXT("UnregisterInvoker")){if(Invoker.IsValid()){N->UnregisterInvoker(*Invoker);if(!Invoker->Destroy()){D(Context.Token,WorldError(TEXT("Demo.Nav.InvokerDestroy"),TEXT("本Invoker销毁失败。")));return;}Invoker.Reset();}D(Context.Token,FGamePlatformResult::Success());return;}
  const FVector Start(A.Number(TEXT("StartX")),0,0),Goal(A.Number(TEXT("GoalX")),0,0);const double Radius=A.Number(TEXT("Radius")),Timeout=A.Number(TEXT("Timeout"));const FVector Position(A.Number(TEXT("X")),A.Number(TEXT("Y")),0);
  Load(A.Asset(TEXT("AgentProfile")),[Weak=TWeakPtr<FWorldManualState>(AsShared()),O,Start,Goal,Radius,Timeout,Position](UObject* Object){
   auto P=Weak.Pin();auto* Profile=Cast<UGamePlatformNavigationAgentProfile>(Object);if(!P||!P->Alive()||!Profile)return WorldError(TEXT("Demo.Nav.Profile"),TEXT("租约资源不是合法导航AgentProfile。"));auto* N=P->World->GetSubsystem<UGamePlatformNavigationWorldSubsystem>();if(!N)return WorldError(TEXT("Demo.Nav.Missing"),TEXT("导航服务失效。"));
   if(!P->bOwnNavProfile){if(!N->RegisterAgentProfile(Profile))return WorldError(TEXT("Demo.Nav.ProfileBusy"),TEXT("同名Profile已存在或配置非法；拒绝接管。"));P->NavProfile=Profile;P->bOwnNavProfile=true;}else if(P->NavProfile.Get()!=Profile)return WorldError(TEXT("Demo.Nav.ProfileChanged"),TEXT("先清理原Profile才能更换。"));
   if(O==TEXT("RegisterInvoker")){if(P->Invoker.IsValid())return WorldError(TEXT("Demo.Nav.InvokerBusy"),TEXT("本Invoker已登记。"));P->Invoker=P->SpawnAnchor(Position);EGamePlatformNavigationError Error=EGamePlatformNavigationError::None;const bool B=P->Invoker.IsValid()&&N->RegisterInvoker(*P->Invoker,Profile->ProfileId,Error);P->Observe(FString::Printf(TEXT("真实RegisterInvoker=%d Error=%d；动态NavData按原生系统生成。"),B,int32(Error)));return WorldBool(B,TEXT("RegisterInvoker"));}
   if(O==TEXT("Project")){auto V=N->ProjectPoint(Start,FVector(100,100,200),Profile->ProfileId,Profile->DefaultFilterId);P->Observe(FString::Printf(TEXT("真实Project Success=%d Error=%d Position=%s"),V.bSuccess,int32(V.Error),*V.ProjectedLocation.ToString()));return WorldBool(V.bSuccess,TEXT("ProjectPoint"));}
   if(O==TEXT("RandomReachable")){FVector Found;EGamePlatformNavigationError Error=EGamePlatformNavigationError::None;const bool B=N->FindRandomReachablePoint(Start,Radius,Profile->ProfileId,Profile->DefaultFilterId,Found,Error);P->Observe(FString::Printf(TEXT("真实RandomReachable Success=%d Error=%d Position=%s"),B,int32(Error),*Found.ToString()));return WorldBool(B,TEXT("RandomReachable"));}
   FGamePlatformNavigationRequest Q;Q.RequestId=FGuid::NewGuid();Q.WorldGeneration=N->GetWorldGeneration();Q.AgentProfileId=Profile->ProfileId;Q.FilterId=Profile->DefaultFilterId;Q.Start=Start;Q.Goal=Goal;Q.TimeoutSeconds=Timeout;Q.Authority=EGamePlatformNavigationAuthority::ServerAuthoritative;
   if(!P->Anchor.IsValid())P->Anchor=P->SpawnAnchor(Start);Q.OwnerScope=P->Anchor.Get();
   if(O==TEXT("TestPath")){EGamePlatformNavigationError Error=EGamePlatformNavigationError::None;const bool B=N->TestPath(Q,Error);P->Observe(FString::Printf(TEXT("真实TestPath Success=%d Error=%d"),B,int32(Error)));return WorldBool(B,TEXT("TestPath"));}
   if(O==TEXT("FindPath")){auto V=N->FindPath(Q);P->Observe(FString::Printf(TEXT("真实FindPath Status=%d Error=%d Points=%d Length=%.2f Cost=%.2f"),int32(V.Status),int32(V.Error),V.PathPoints.Num(),V.PathLength,V.PathCost));return WorldBool(V.IsSuccess(),TEXT("FindPath"));}
   if(O==TEXT("FindPathAsync")){
    if(P->PathRequest.IsValid())return WorldError(TEXT("Demo.Nav.QueryBusy"),TEXT("先取消上一请求。"));const auto E=P->Epoch;const auto ExpectedId=Q.RequestId;const int32 G=Q.WorldGeneration;
    // Engine may reject synchronously; capture terminal values before publishing handle, then reconcile after assignment.
    auto Terminal=MakeShared<TOptional<FGamePlatformNavigationPathResult>>();auto Submitting=MakeShared<bool>(true);
    auto H=N->FindPathAsync(Q,FGamePlatformNavigationQueryCompleted::CreateLambda([Weak,E,ExpectedId,G,Terminal,Submitting](const auto& V){if(*Submitting){*Terminal=V;return;}auto S=Weak.Pin();if(!S||!S->Current(E)||S->PathRequest.RequestId!=ExpectedId||S->PathRequest.WorldGeneration!=G||V.RequestId!=ExpectedId||V.WorldGeneration!=G)return;S->PathRequest={};S->Observe(FString::Printf(TEXT("真实异步导航终态 Status=%d Error=%d Points=%d"),int32(V.Status),int32(V.Error),V.PathPoints.Num()));}));
    P->PathRequest=H;*Submitting=false;if(!P->Current(E)){if(H.IsValid())N->CancelRequest(H);return FGamePlatformResult::Cancelled(TEXT("异步导航提交期间已停止。"));}
    if(Terminal->IsSet()){const auto V=Terminal->GetValue();P->PathRequest={};P->Observe(FString::Printf(TEXT("真实同步导航拒绝/终态 Error=%d"),int32(V.Error)));return WorldBool(V.IsSuccess(),TEXT("FindPathAsync"));}return WorldBool(H.IsValid(),TEXT("FindPathAsync"));
   }return WorldError(TEXT("Demo.Nav.Operation"),TEXT("未知导航操作。"));
  },MoveTemp(D));return;
 }
 if(Feature==TEXT("GamePlatformPCG.authoring")||Feature==TEXT("GamePlatformPCG.spatial_bake")){
  FString Error;if(O==TEXT("CreateFoundations")){D(Context.Token,EditorResult(GamePlatformPCGManualAuthoring::CreateFoundations(Error),Error));return;}
  if(O==TEXT("CreatePlacements")){D(Context.Token,EditorResult(GamePlatformPCGManualAuthoring::CreateManualPlacements(Error),Error));return;}
  if(O==TEXT("CreateRealizedGraph")){
   const FSoftObjectPath ProfilePath=A.Asset(TEXT("Profile")),MeshSetPath=A.Asset(TEXT("MeshSet"));const FName OutputName=A.Id(TEXT("OutputName"));
   const auto NameResult=ValidateManualRealizedName(OutputName);if(!NameResult.IsSuccess()){D(Context.Token,NameResult);return;}
   Load(ProfilePath,[Weak=TWeakPtr<FWorldManualState>(AsShared()),ProfilePath,MeshSetPath,OutputName](UObject*){
    auto P=Weak.Pin();if(!P||!P->Alive()||!P->Data())return FGamePlatformResult::Cancelled(TEXT("作者准备完成后来源失效"));
    const auto E=P->Epoch;FString Saved;FGamePlatformResult Result;
    {
     TGuardValue<bool> Authoring(P->bAuthoringGraph,true); // AssetCreated/Save回调Stop必须保留完整租约到本栈返回。
     Result=GamePlatformPCGManualAuthoring::CreateRealizedGraph(*P->Data(),P->DefinitionLease,ProfilePath,MeshSetPath,OutputName,
      [Weak,E]{auto S=Weak.Pin();return S&&S->Current(E);},Saved);
    }
    P->SavedRealizedGraph=Saved; // 仅本旧State持久产物诊断，不能发布到后继Run。
    if(P->Current(E))P->Observe(FString::Printf(TEXT("Realized作者结果 Code=%s Saved=%s；固定保存于/Game/Development/Foundation/PCG/Realized/，Stop不删除持久图。源Profile/模板只读，绑定/Cook/人工签收另验。"),*Result.Code.ToString(),*Saved));
    return Result;
   },MoveTemp(D));return;
  }
  Load(A.Asset(TEXT("Profile")),[Weak=TWeakPtr<FWorldManualState>(AsShared()),O,Priority=static_cast<int32>(A.Int(TEXT("Priority"))),MaskPosition=FVector(A.Number(TEXT("MaskX")),A.Number(TEXT("MaskY")),0),MaskExtent=A.Number(TEXT("MaskExtent"))](UObject* Obj){auto P=Weak.Pin();auto* Profile=Cast<UGamePlatformPCGProfileDefinition>(Obj);if(!P||!P->Alive()||!Profile)return WorldError(TEXT("Demo.PCG.Profile"),TEXT("资源不是PCG Profile。"));FString E;
   if(O==TEXT("ValidateProfile"))return EditorResult(GamePlatformPCGManualAuthoring::ValidateProfile(Profile,E),E);
   if(O==TEXT("InspectSource")){FString F;TArray<FString> Deps;const bool B=GamePlatformPCGManualAuthoring::InspectSource(Profile,F,Deps,E);P->Observe(FString::Printf(TEXT("真实PCG源 Fingerprint=%s Dependencies=%d；未执行Bake或重开验收。"),*F,Deps.Num()));return EditorResult(B,E);}
   if(O==TEXT("ConfigureSpatialMasks")){
    if(!P->Director.IsValid()){const auto Epoch=P->Epoch;auto* Created=P->World->SpawnActor<AGamePlatformPCGWorldDirector>();if(!P->Current(Epoch)){if(Created)Created->Destroy();return FGamePlatformResult::Cancelled(TEXT("Director出生期间已停止。"));}P->Director=Created;if(Created)Created->SetFlags(RF_Transient);}
    if(!P->Director.IsValid())return WorldError(TEXT("Demo.PCG.DirectorSpawn"),TEXT("本真实Director创建失败。"));
    if(!P->Exclusion.IsValid()){const auto Epoch=P->Epoch;auto* Created=P->World->SpawnActor<AGamePlatformPCGExclusionActor>();if(!P->Current(Epoch)){if(Created)Created->Destroy();return FGamePlatformResult::Cancelled(TEXT("排除参与者出生期间已停止。"));}P->Exclusion=Created;if(Created){Created->SetFlags(RF_Transient);Created->DomainId=TEXT("ManualLock");const bool Registered=P->Director->RegisterParticipant(Created);P->bExclusionRegistered=Registered;if(!P->Current(Epoch)){if(Registered&&P->Director.IsValid()&&P->Director->UnregisterParticipant(Created))P->bExclusionRegistered=false;return FGamePlatformResult::Cancelled(TEXT("参与者登记期间来源失活；保留未撤销的完整归属。"));}if(!Registered)return WorldError(TEXT("Demo.PCG.ParticipantRegistration"),TEXT("真实Director拒绝本参与者。"));}}
    if(!P->Exclusion.IsValid())return WorldError(TEXT("Demo.PCG.ExclusionSpawn"),TEXT("本真实空间参与者创建失败。"));
    P->Exclusion->Bounds->SetBoxExtent(FVector(MaskExtent,MaskExtent,100));P->Exclusion->SetActorLocation(MaskPosition);
    auto* Graph=Profile->GraphReference.Get();if(!Graph)return WorldError(TEXT("Demo.PCG.GraphResourceMissing"),TEXT("Data的PCGGeneration租约未加载源图。"));
    P->SpatialGraph.Reset(DuplicateObject<UPCGGraph>(Graph,GetTransientPackage()));P->SpatialGraph->SetFlags(RF_Transient);
    return EditorResult(GamePlatformPCGManualAuthoring::ConfigureOwnedSpatialMasks(P->SpatialGraph.Get(),P->Director.Get(),Priority,E),E);
   }return WorldError(TEXT("Demo.PCG.EditorOperation"),TEXT("未知PCG Editor操作。"));
  },MoveTemp(D));return;
 }
 if(Feature==TEXT("GamePlatformDeveloperTools.asset_audit")){
  FString Error;bool B=false;const auto Path=A.Asset(TEXT("Asset"));
  if(O==TEXT("AuditFolder"))B=FGamePlatformAssetAudit::AuditFolder(FName(*FPaths::GetPath(Path.GetLongPackageName())),AssetRows,Error);
  else if(O==TEXT("AuditDependencies")){TArray<FName> Deps;B=FGamePlatformAssetAudit::AuditDependencies(FName(*Path.GetLongPackageName()),Deps,Error);Observe(FString::Printf(TEXT("真实依赖审计 Count=%d；不以数量代替规则/运行验收。"),Deps.Num()));}
  else if(O==TEXT("WriteCsv"))B=FGamePlatformAssetAudit::WriteCsv(AssetRows,WorldOutput(Context.Token)/TEXT("AssetAudit.csv"),Error);
  D(Context.Token,EditorResult(B,Error));return;
 }
 if(Feature==TEXT("GamePlatformDeveloperTools.validation")){
  FString E;bool B=true;
  if(O==TEXT("RegisterRules")){if(!RuleProvider.IsNone())B=false;else{const FName Own(*(TEXT("Manual.World.")+Context.Token.RunId.ToString(EGuidFormats::Digits)));B=FGamePlatformValidationService::RegisterRuleProvider(MakeShared<FWorldManualRules>(Own),E);if(B)RuleProvider=Own;}}
  else if(O==TEXT("UnregisterRules")){if(!RuleProvider.IsNone())FGamePlatformValidationService::UnregisterRuleProvider(RuleProvider);RuleProvider=NAME_None;}
  else if(O==TEXT("RulesSnapshot")){auto Rules=FGamePlatformValidationService::GetAllRulesSnapshot();Observe(FString::Printf(TEXT("真实验证规则快照 Count=%d；含外部内建项，不撤销它们。"),Rules.Num()));}
  else{FGamePlatformValidationResult V;V.RuleId=RuleProvider;V.Target=TEXT("Manual.World.Fixture");V.Status=A.Bool(TEXT("Passed"))?EGamePlatformValidationStatus::Passed:EGamePlatformValidationStatus::Failed;V.Message=TEXT("人工页门禁夹具输入；不是正式资产验证结果。");if(O==TEXT("GateResult"))Observe(FString::Printf(TEXT("真实ShouldBlockResult=%d Stage=%s"),FGamePlatformValidationService::ShouldBlockResult(V,A.Id(TEXT("GateStage"))),*A.Id(TEXT("GateStage")).ToString()));else if(O==TEXT("WriteReports")){FGamePlatformValidationRunSummary Summary;Summary.RunId=FGamePlatformValidationService::CreateRunId();Summary.StartedAt=Summary.FinishedAt=FDateTime::UtcNow();Summary.Results.Add(V);FString Directory;B=FGamePlatformValidationService::WriteReports(Summary,Directory,E);}}
  D(Context.Token,EditorResult(B,E));return;
 }
 if(Feature==TEXT("GamePlatformDeveloperTools.human_review")){
  if(O==TEXT("ValidateEvidence")){FString E;auto* S=GEditor?GEditor->GetEditorSubsystem<UGamePlatformReviewSubsystem>():nullptr;D(Context.Token,EditorResult(S&&Review.IsValid()&&Review->BoundCase&&S->ValidateReviewEvidence(*Review->BoundCase,*Review,E),E));return;}
  Load(A.Asset(TEXT("ReviewCase")),[Weak=TWeakPtr<FWorldManualState>(AsShared())](UObject* O){auto P=Weak.Pin();auto* Case=Cast<UGamePlatformReviewCase>(O);auto* S=GEditor?GEditor->GetEditorSubsystem<UGamePlatformReviewSubsystem>():nullptr;if(!P||!P->Alive()||!S||!Case)return WorldError(TEXT("Demo.Review.Host"),TEXT("需要真实Editor ReviewCase和ReviewSubsystem。"));P->Review.Reset(S->CreatePendingReport(Case,P->Context.Token.RunId.ToString(EGuidFormats::DigitsWithHyphens),TEXT("manual.pending.binary"),TEXT("manual.pending.content")));return WorldBool(P->Review.IsValid(),TEXT("CreatePendingReport"));},MoveTemp(D));return;
 }
 if(Feature==TEXT("GamePlatformDeveloperTools.performance")){
  const double Duration=A.Number(TEXT("Duration"));const int32 Players=static_cast<int32>(A.Int(TEXT("Players")));
  Load(A.Asset(TEXT("Profile")),[Weak=TWeakPtr<FWorldManualState>(AsShared()),O,Duration,Players](UObject* Obj){auto P=Weak.Pin();auto* Asset=Cast<UGamePlatformPerformanceProfileAsset>(Obj);if(!P||!P->Alive()||!Asset)return WorldError(TEXT("Demo.Performance.Profile"),TEXT("需要真实性能Profile资产。"));auto Profile=Asset->Profile;Profile.DurationSeconds=Duration;Profile.PlayerCount=Players;
   if(!Profile.ScenarioId.ToString().StartsWith(TEXT("Manual.World."))||Profile.Map.GetLongPackageName()!=TEXT("/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub"))return WorldError(TEXT("Demo.Performance.Scope"),TEXT("仅允许Manual.World场景和当前开发Hub，拒绝生产场景。"));
   if(!P->Runner.IsValid())P->Runner.Reset(NewObject<UGamePlatformPerformanceTestRunner>());FString E;bool B=false;
   if(O==TEXT("ValidateProfile"))B=P->Runner->IsProfileValid(Profile,E);
   else if(O==TEXT("ExecuteRegistered")){TMap<FName,TArray<double>> Samples;B=FGamePlatformPerformanceExecutorRegistry::Execute(Profile,Samples,E);P->Observe(FString::Printf(TEXT("真实已注册性能Executor：Success=%d Metrics=%d；未注册时明确拒绝，不生成虚假样本。"),B,Samples.Num()));}
   else if(O==TEXT("RunProfile")){B=P->Runner->RunProfile(Profile,TEXT("manual.pending.binary"),TEXT("manual.pending.content"),FEngineVersion::Current().ToString(),TEXT("manual.unmeasured"),[](const auto& V,auto& Samples,auto& Error){return FGamePlatformPerformanceExecutorRegistry::Execute(V,Samples,Error);},P->Performance,E);P->bHasPerformance=B;}
   else if(O==TEXT("WriteResult")){B=P->bHasPerformance&&P->Runner->WriteResultJson(P->Performance,WorldOutput(P->Context.Token)/TEXT("Performance.json"),E);}
   else if(O==TEXT("CompareBaseline")){FGamePlatformPerformanceBaseline A,Baseline;A.ScenarioId=Profile.ScenarioId;A.BuildVersion=TEXT("manual.A");Baseline=A;Baseline.HardwareProfile=TEXT("changed.hardware");B=!P->Runner->CanCompareBaselines(A,Baseline,E);}
   return EditorResult(B,E);
  },MoveTemp(D));return;
 }
 D(Context.Token,WorldError(TEXT("Demo.World.EditorOperation"),TEXT("Editor操作未在封闭领域接线。")));
#endif
}
}

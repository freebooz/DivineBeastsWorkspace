// SSLM项目层Editor回归：引擎Play Session/GI/LP、Monolith样板、真实供应商/事务/事件。
// 仅开发Editor；自动化latent Update等待引擎事件，不属于UI轮询；不连接后端，不改用户图或键位。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Scenarios/DivineBeastsDemoEditorScenarios.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Manual/GamePlatformEditorResources.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Blueprint.h"
#include "Blueprint/UserWidget.h"
#include "UObject/StrongObjectPtr.h"

#include "ScopedTransaction.h"
#include "GlooPrintManual.h"
#include "UObject/UObjectIterator.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "EdGraphSchema_K2.h"
#include "Materials/Material.h"
#include "MaterialGraph/MaterialGraph.h"
#include "GraphEditor.h"
#include "SGraphPanel.h"
using namespace DivineBeasts::Demo;
namespace {
/** 单人真实PIE中顺序运行全部九组；关闭窗口和异步格式化均等真实终态，超时失败。 */
class FEditorAdapterLifecycle final:public IAutomationLatentCommand {
 FAutomationTestBase* T;double Deadline=FPlatformTime::Seconds()+90;
 TUniquePtr<IDivineBeastsDemoScenarioAdapter> Adapter;TStrongObjectPtr<UUserWidget> Widget;
 FDivineBeastsDemoScenarioContext C;TArray<FDivineBeastsDemoParameterValue> Values;
 TArray<FName> Features=EditorManualFeatures();int32 Index=0,Stage=0,Observations=0,StopCalls=0;

 enum class ETerminal:uint8 {None,Pending,Succeeded,Failed,Cancelled};
 ETerminal Terminal=ETerminal::None;FGuid TerminalRequest,SuccessfulRequest;FString TerminalReason;
 TWeakObjectPtr<UEdGraph> OwnedGraph;TMap<FGuid,FIntPoint> BeforeGraph;TSet<FGuid> SelectedBefore;
 FGuid ExternalTransaction;TStrongObjectPtr<UBlueprint> External;
 TMap<FGuid,FIntPoint> Positions()const{TMap<FGuid,FIntPoint> Out;if(OwnedGraph.IsValid())for(UEdGraphNode* N:OwnedGraph->Nodes)if(IsValid(N))Out.Add(N->NodeGuid,{N->NodePosX,N->NodePosY});return Out;}
 bool SamePositions(const TMap<FGuid,FIntPoint>& A,const TMap<FGuid,FIntPoint>& B)const{if(A.Num()!=B.Num())return false;for(const auto& V:A){const auto* Q=B.Find(V.Key);if(!Q||*Q!=V.Value)return false;}return true;}


 /** MakeUniqueObjectName在Editor追加FName编号；只忽略编号比较完整Run基名，再按绑定和Owner资格选唯一活跃图。 */
 bool FindOwnedGraph(){
  OwnedGraph.Reset();const FString Run=C.Token.RunId.ToString(EGuidFormats::Digits);TSet<UObject*> ActiveRoots;int32 Graphs=0;
  const FName BlueprintBase(*(TEXT("DBAManualGraph_")+Run)),MaterialBase(*(TEXT("DBAManualMaterial_")+Run));
  const bool bMaterial=!Values.IsEmpty()&&Values[0].IntegerValue==1; // Gloo封闭合同的首参数为GraphKind。
  auto ConsiderActiveGraph=[&](UObject* Root,UEdGraph* Graph){
   if(!IsValid(Graph)||Graph->GetOuter()!=Root)return;
   const auto Editor=SGraphEditor::FindGraphEditorForGraph(Graph);auto* Panel=Editor?Editor->GetGraphPanel():nullptr;
   if(!Editor||Editor->GetCurrentGraph()!=Graph||!Panel||Panel->GetGraphObj()!=Graph)return;
   FString Reason;if(!GlooPrintManual::IsGraphEligible(*Graph,Reason))return;
   // Reset后事务可能保留同基名旧Root：没有当前绑定和精确供应商资格的旧图不能计作活跃候选。
   ActiveRoots.Add(Root);++Graphs;OwnedGraph=Graph;
  };
  if(!bMaterial){for(TObjectIterator<UBlueprint> It;It;++It)if(IsValid(*It)&&It->GetFName().IsEqual(BlueprintBase,ENameCase::CaseSensitive,false)&&It->GetOutermost()==GetTransientPackage()){
   for(UEdGraph* Graph:It->UbergraphPages)if(IsValid(Graph)&&Graph->GetOuter()==*It&&Graph->GetFName()==TEXT("ManualGraph"))ConsiderActiveGraph(*It,Graph);
  }}else{for(TObjectIterator<UMaterial> It;It;++It)if(IsValid(*It)&&It->GetFName().IsEqual(MaterialBase,ENameCase::CaseSensitive,false)&&It->GetOutermost()==GetTransientPackage()){
   ConsiderActiveGraph(*It,It->MaterialGraph.Get());
  }}
  T->TestEqual(TEXT("精确Run基名对应唯一活跃Root"),ActiveRoots.Num(),1);T->TestEqual(TEXT("活跃Root内对应唯一已绑定且获资格的作者图"),Graphs,1);
  if(ActiveRoots.Num()!=1||Graphs!=1||!OwnedGraph.IsValid()){OwnedGraph.Reset();return false;}
  const auto Editor=SGraphEditor::FindGraphEditorForGraph(OwnedGraph.Get());auto* Panel=Editor?Editor->GetGraphPanel():nullptr;
  const bool Bound=Editor&&Editor->GetCurrentGraph()==OwnedGraph.Get()&&Panel&&Panel->GetGraphObj()==OwnedGraph.Get();
  T->TestTrue(TEXT("位置/选择取样图就是实际SGraphEditor及GraphPanel绑定图"),Bound);
  // 后续仍保留同一Owner资格和严格位移/锚点/Undo断言；多重活跃候选失败，不任取扫描首项。
  if(!Bound){OwnedGraph.Reset();return false;}return true;
 }
 void ReadObservation(const FDivineBeastsDemoRunToken& Token,const FString& Text){
  T->TestTrue(TEXT("观察保持完整Token"),Token==C.Token);++Observations;
  if(Text.Contains(TEXT("终态=Succeeded ")))Terminal=ETerminal::Succeeded;else if(Text.Contains(TEXT("终态=Failed ")))Terminal=ETerminal::Failed;else if(Text.Contains(TEXT("终态=Cancelled ")))Terminal=ETerminal::Cancelled;else if(Text.Contains(TEXT("终态=Pending ")))Terminal=ETerminal::Pending;
  FString Tail;if(Text.Split(TEXT("Request="),nullptr,&Tail)){FString Id;Tail.Split(TEXT(" "),&Id,nullptr);FGuid::Parse(Id,TerminalRequest);}if(Text.Split(TEXT("原因="),nullptr,&Tail))TerminalReason=Tail;
 }
 FGamePlatformResult Call(FName Op){FGamePlatformResult R=FGamePlatformResult::Failure(TEXT("Test.NoCallback"),TEXT("没有同步操作完成"));int32 Count=0;Adapter->Execute(Op,Values,[&](const auto& Token,const auto& Result){++Count;T->TestTrue(TEXT("完成保持完整Token"),Token==C.Token);R=Result;});T->TestEqual(TEXT("同步命令恰好一次完成"),Count,1);return R;}
public:explicit FEditorAdapterLifecycle(FAutomationTestBase& Test):T(&Test){Features.Add(TEXT("GlooPrint.format"));}
 bool Update()override{
  if(FPlatformTime::Seconds()>Deadline){T->AddError(TEXT("Editor案例未在90秒收敛；保持失败，不以受理当完成。"));if(Adapter)Adapter->Stop([](const auto&,const auto&){});return true;}
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr;
  if(!W||!W->HasBegunPlay())return false;
  auto* GI=W->GetGameInstance();auto* LP=GI?GI->GetFirstGamePlayer():nullptr;if(!LP)return false;
  if(!Widget.IsValid()){
   auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/WBP_DBA_PluginDemoContextSample.WBP_DBA_PluginDemoContextSample_C"));
   if(!Class){T->AddError(TEXT("缺已由Monolith制作的原生ContextSample；不以原生替代树跳过。"));return true;}
   Widget.Reset(CreateWidget<UUserWidget>(LP->GetPlayerController(W),Class));if(!Widget.IsValid()){T->AddError(TEXT("真实LP样板创建失败"));return true;}
  }
  if(Index==Features.Num()){Widget.Reset();return true;}
  if(Stage==0){
   C={};C.Token={FGuid::NewGuid(),FGuid::NewGuid(),1};C.World=W;C.Scenario=EditorManualContract(Features[Index]);C.SampleWidget=Widget.Get();
   Values.Reset();for(const auto& P:C.Scenario.Parameters)Values.Add(P.DefaultValue);if(Index==9)Values[0].IntegerValue=1;
   SelectedBefore.Reset();Observations=0;Terminal=ETerminal::None;TerminalRequest.Invalidate();SuccessfulRequest.Invalidate();TerminalReason.Reset();StopCalls=0;OwnedGraph.Reset();
   C.PublishObservation=[this](const auto& Token,const FString& Text){ReadObservation(Token,Text);};
   Adapter=CreateEditorManualScenario(*W,Features[Index]);FGamePlatformResult Prepared;Adapter->Prepare(C,[&](const auto&,const auto& R){Prepared=R;});T->TestTrue(TEXT("真实PIE准备供应商页"),Prepared.IsSuccess());if(!Prepared.IsSuccess()){Stage=4;return false;}
   T->TestEqual(TEXT("首次绑定只读完整快照"),Observations,1);
   Adapter->Prepare(C,[&](const auto&,const auto& R){T->TestFalse(TEXT("适配器禁止重复绑定"),R.IsSuccess());});
   T->TestTrue(TEXT("真实快捷键注册链检查"),Call(TEXT("InspectShortcuts")).IsSuccess());
   const FName Op=Features[Index].ToString().StartsWith(TEXT("GlooPrint."))?TEXT("QueueFormat"):Features[Index]==TEXT("BlueprintAutoLayout.selected")?TEXT("LayoutSelected"):Features[Index]==TEXT("BlueprintAutoLayout.group")?TEXT("LayoutGroup"):Features[Index]==TEXT("BlueprintAutoLayout.route")?TEXT("LayoutRoute"):TEXT("LayoutWhole");
   T->TestFalse(TEXT("无图明确拒绝真实操作"),Call(Op).IsSuccess());
   if(!Values.IsEmpty()){const auto Old=Values[0];Values[0].IntegerValue=2147483647;T->TestFalse(TEXT("非法封闭参数拒绝"),Call(TEXT("CreateGraph")).IsSuccess());Values[0]=Old;}
   T->TestTrue(TEXT("创建本Run真实Blueprint图与SGraphEditor"),Call(TEXT("CreateGraph")).IsSuccess());Stage=1;return false;
  }
  if(Stage==1){
   // 真实Slate已布局一帧；图/选择通知会增加观察，不依赖UI计时器。
   if(Features[Index]==TEXT("BlueprintAutoLayout.selected")){T->TestTrue(TEXT("清空真实选择"),Call(TEXT("ClearSelection")).IsSuccess());T->TestFalse(TEXT("无选择不能成功"),Call(TEXT("LayoutSelected")).IsSuccess());}
   T->TestTrue(TEXT("真实图选择"),Call(TEXT("SelectAnchors")).IsSuccess());
   if(C.Scenario.Operations.Contains(TEXT("ApplySettings"))){const int32 Before=Observations;T->TestTrue(TEXT("供应商真实设置"),Call(TEXT("ApplySettings")).IsSuccess());T->TestTrue(TEXT("设置事件更新UI快照"),Observations>Before);}

   if(C.Scenario.Operations.Contains(TEXT("QueueFormat"))){
    if(!FindOwnedGraph()){T->AddError(TEXT("未找到本Run唯一、已绑定原生编辑器的作者图"));Stage=4;return false;}BeforeGraph=Positions();
    if(auto Editor=SGraphEditor::FindGraphEditorForGraph(OwnedGraph.Get()))for(UObject* O:Editor->GetSelectedNodes())if(auto* N=Cast<UEdGraphNode>(O))SelectedBefore.Add(N->NodeGuid);
    FString Reason;T->TestTrue(TEXT("拥有图在真实PIE通过同一供应商资格"),GlooPrintManual::IsGraphEligible(*OwnedGraph.Get(),Reason));
    // 一个真实合法且同样Transient的非自有Blueprint仍不能在PIE使用供应商入口。
    TStrongObjectPtr<UBlueprint> Other(FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("UnownedPIEGraph")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass()));
    auto* OtherGraph=FBlueprintEditorUtils::CreateNewGraph(Other.Get(),TEXT("OrdinaryGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());FBlueprintEditorUtils::AddUbergraphPage(Other.Get(),OtherGraph);
    T->TestFalse(TEXT("普通非自有图保留PIE保护"),GlooPrintManual::IsGraphEligible(*OtherGraph,Reason));
    Terminal=ETerminal::None;T->TestTrue(TEXT("供应商原生分片任务受理"),Call(TEXT("QueueFormat")).IsSuccess());T->TestTrue(TEXT("真实在途请求身份已发布"),TerminalRequest.IsValid());SuccessfulRequest=TerminalRequest;Stage=2;return false;
   }
   for(FName O:{FName(TEXT("LayoutWhole")),FName(TEXT("LayoutSelected")),FName(TEXT("LayoutGroup")),FName(TEXT("LayoutRoute")),FName(TEXT("LayoutGroupRoute"))})if(C.Scenario.Operations.Contains(O))T->TestTrue(TEXT("真实供应商同步操作"),Call(O).IsSuccess());
   if(Features[Index]==TEXT("BlueprintAutoLayout.whole"))T->TestTrue(TEXT("仅撤销当前本Run真实事务"),Call(TEXT("Undo")).IsSuccess());
   T->TestTrue(TEXT("真实图变化通知可观察"),Observations>2);Stage=3;return false;
  }

  if(Stage==2){
   if(Terminal==ETerminal::None||Terminal==ETerminal::Pending)return false;
   T->TestTrue(TEXT("正常Blueprint/Material必须精确Succeeded"),Terminal==ETerminal::Succeeded);T->TestTrue(TEXT("终态请求身份与本次提交完全一致"),TerminalRequest==SuccessfulRequest);
   if(Terminal!=ETerminal::Succeeded){T->AddError(TEXT("正常格式化失败原因：")+TerminalReason);Stage=4;return false;}
   const auto After=Positions();T->TestFalse(TEXT("正常格式化确有真实节点位置改变"),SamePositions(BeforeGraph,After));
   bool AnchorStayed=false;for(const auto& Id:SelectedBefore){const auto* A=BeforeGraph.Find(Id);const auto* B=After.Find(Id);AnchorStayed|=A&&B&&*A==*B;}T->TestTrue(TEXT("原生选中锚点至少一个保持位置"),AnchorStayed);
   T->TestTrue(TEXT("Gloo成功事务允许本Run一次Undo"),Call(TEXT("Undo")).IsSuccess());T->TestTrue(TEXT("Undo真实恢复全部原节点位置"),SamePositions(BeforeGraph,Positions()));
   T->TestFalse(TEXT("不能再次Undo并撤销前继工具"),Call(TEXT("Undo")).IsSuccess());
   // 原生manual请求先排队到既有FEditor Tick，因此能确定取消真正Pending，而非事后幂等。
   Terminal=ETerminal::None;T->TestTrue(TEXT("单独取消用例受理"),Call(TEXT("QueueFormat")).IsSuccess());const FGuid CancelledRequest=TerminalRequest;T->TestTrue(TEXT("取消对象确是新Pending请求"),Terminal==ETerminal::Pending&&CancelledRequest.IsValid()&&CancelledRequest!=SuccessfulRequest);
   T->TestTrue(TEXT("精确取消真实在途任务"),Call(TEXT("CancelFormat")).IsSuccess());T->TestTrue(TEXT("取消用例必须精确Cancelled"),Terminal==ETerminal::Cancelled&&TerminalRequest==CancelledRequest);T->TestTrue(TEXT("取消不改变原图"),SamePositions(BeforeGraph,Positions()));
   // 关闭供应商功能是明确失败夹具；失败不能充当正常排版或取消成功。
   GlooPrintManual::FSettingsSnapshot Original,Disabled;GlooPrintManual::ReadSettings(Original);Disabled=Original;Disabled.bEnabled=false;FString Error;GlooPrintManual::ApplySettings(Disabled,Error);
   Terminal=ETerminal::None;Call(TEXT("QueueFormat"));T->TestTrue(TEXT("禁用功能精确Failed并报告真实原因"),Terminal==ETerminal::Failed&&TerminalRequest.IsValid()&&TerminalRequest!=CancelledRequest&&TerminalReason.Contains(TEXT("disabled"),ESearchCase::IgnoreCase));T->TestTrue(TEXT("失败不改图"),SamePositions(BeforeGraph,Positions()));T->TestTrue(TEXT("精确恢复本测试禁用设置"),GlooPrintManual::RestoreSettings(Original,Disabled,Error));Stage=3;return false;
  }
  if(Stage==3){
   // 本测试拥有的另一对象模拟用户后继事务；Stop绝不能自动Undo这笔事务。
   External.Reset(NewObject<UBlueprint>(GetTransientPackage(),NAME_None,RF_Transactional));{FScopedTransaction Tx(FText::FromString(TEXT("T06独立后继事务")));External->Modify();External->BlueprintDescription=TEXT("后继状态必须保留");}
   ExternalTransaction=GEditor->Trans->GetUndoContext().TransactionId;
   T->TestFalse(TEXT("本Run拒绝Undo后继事务"),Call(TEXT("Undo")).IsSuccess());Stage=4;return false;
  }
  if(Stage==4){FGamePlatformResult R;Adapter->Stop([&](const auto&,const auto& Result){R=Result;});++StopCalls;if(!R.IsSuccess()){if(StopCalls>10){T->AddError(TEXT("Stop重试未完成，不能释放原Editor租约"));return true;}return false;}
   if(ExternalTransaction.IsValid()){T->TestTrue(TEXT("Stop保留外部后继事务"),GEditor->Trans->GetUndoContext().TransactionId==ExternalTransaction);T->TestEqual(TEXT("后继对象值保留"),External->BlueprintDescription,FString(TEXT("后继状态必须保留")));}
   const int32 Before=Observations;T->TestFalse(TEXT("失活操作拒绝"),Call(TEXT("Snapshot")).IsSuccess());T->TestEqual(TEXT("失活不发布旧快照"),Observations,Before);
   Adapter.Reset();External.Reset();ExternalTransaction.Invalidate();++Index;Stage=0;return false;
  }return false;
 }
};
/** 两个真正引擎PIE GI核对平台资源排他；宿主本身拒绝多PIE。 */
class FTwoPIEResourceCheck final:public IAutomationLatentCommand {
 FAutomationTestBase* T;double Deadline=FPlatformTime::Seconds()+40;
public:explicit FTwoPIEResourceCheck(FAutomationTestBase& In):T(&In){}
 bool Update()override{
  TArray<UWorld*> Worlds;if(GEngine)for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&C.World()&&C.World()->HasBegunPlay()&&C.World()->GetGameInstance())Worlds.Add(C.World());
  if(Worlds.Num()<2){if(FPlatformTime::Seconds()>Deadline){T->AddError(TEXT("引擎未创建两个真实PIE GI"));return true;}return false;}
  auto* A=Worlds[0]->GetGameInstance();auto* B=Worlds[1]->GetGameInstance();T->TestTrue(TEXT("两个GI确实不同"),A!=B);
  auto* R=IGamePlatformEditorResources::Get();if(!R){T->AddError(TEXT("缺真实DeveloperTools模块"));return true;}
  FGamePlatformEditorResourceLease First,Second;FString Error;
  T->TestTrue(TEXT("第一GI获取Editor资源"),R->Acquire(*A,*Worlds[0],FGuid::NewGuid(),FGuid::NewGuid(),1,First,Error));
  T->TestFalse(TEXT("第二GI/World不允许同时操作全局Editor"),R->Acquire(*B,*Worlds[1],FGuid::NewGuid(),FGuid::NewGuid(),1,Second,Error));
  if(First.IsValid())T->TestTrue(TEXT("只按第一完整租约释放"),R->Release(First,Error));
  if(auto* Host=A->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>())T->TestFalse(TEXT("多PIE即使资源空闲也不扩大宿主资格"),Host->AcquireEditorFixture({FGuid::NewGuid(),FGuid::NewGuid(),1},*Worlds[0],TEXT("GlooPrint.format")).IsSuccess());else T->AddError(TEXT("真实GI缺ManualHost"));return true;
 }
};

/** 同步选择通知内Stop、完整旧租约阻塞后继、重进及真实引擎World退出，均在真实PIE发生。 */
class FEditorStopAndExit final:public IAutomationLatentCommand {
 FAutomationTestBase* T;double Deadline=FPlatformTime::Seconds()+60;int32 Stage=0,Observations=0,FrozenObservations=0;
 bool Trigger=false,StoppedInside=false;TStrongObjectPtr<UUserWidget> Widget;TStrongObjectPtr<UDivineBeastsDemoManualHostSubsystem> Host;
 TWeakObjectPtr<UWorld> ExitedWorld;TWeakObjectPtr<UGameInstance> ExitedGI;
 TWeakObjectPtr<UUserWidget> ExitedWidget;TWeakObjectPtr<UDivineBeastsDemoManualHostSubsystem> ExitedHost;
 TUniquePtr<IDivineBeastsDemoScenarioAdapter> Adapter;FDivineBeastsDemoScenarioContext C;TArray<FDivineBeastsDemoParameterValue> Values;
 FGamePlatformResult Call(FName Op){FGamePlatformResult R=FGamePlatformResult::Failure(TEXT("Test.MissingCompletion"),TEXT("缺完成"));Adapter->Execute(Op,Values,[&](const auto&,const auto& V){R=V;});return R;}
 bool Prepare(UWorld& W,FName Feature){
  C={};C.Token={FGuid::NewGuid(),FGuid::NewGuid(),1};C.World=&W;C.Scenario=EditorManualContract(Feature);C.SampleWidget=Widget.Get();Values.Reset();for(const auto& P:C.Scenario.Parameters)Values.Add(P.DefaultValue);
  C.PublishObservation=[this](const auto& Token,const FString&){++Observations;T->TestTrue(TEXT("通知token属于当前Run"),Token==C.Token);if(Trigger){Trigger=false;Adapter->Stop([this](const auto&,const auto& R){StoppedInside=true;T->TestFalse(TEXT("同步供应商栈内Stop保留归属待重试"),R.IsSuccess());});}};
  Adapter=CreateEditorManualScenario(W,Feature);FGamePlatformResult R;Adapter->Prepare(C,[&](const auto&,const auto& V){R=V;});return R.IsSuccess();
 }
public:explicit FEditorStopAndExit(FAutomationTestBase& In):T(&In){}
 bool Update()override{
  if(FPlatformTime::Seconds()>Deadline){T->AddError(TEXT("Stop/World退出未收敛；禁止记成功。"));return true;}
  UWorld* W=GEditor?GEditor->PlayWorld:nullptr;
  if(Stage<4&&(!W||!W->HasBegunPlay()))return false;
  if(Stage==0){auto* GI=W->GetGameInstance();auto* LP=GI?GI->GetFirstGamePlayer():nullptr;if(!LP)return false;auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/DBAUIPack_Core/Development/PluginDemos/UI/WBP_DBA_PluginDemoContextSample.WBP_DBA_PluginDemoContextSample_C"));if(!Class){T->AddError(TEXT("缺Monolith真实样板"));return true;}Widget.Reset(CreateWidget<UUserWidget>(LP->GetPlayerController(W),Class));Host.Reset(GI->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>());
   if(!Widget.IsValid()||!Host.IsValid()||!Prepare(*W,TEXT("BlueprintAutoLayout.selected"))){T->AddError(TEXT("真实PIE准备失败"));return true;}
   T->TestTrue(TEXT("创建实际图样板"),Call(TEXT("CreateGraph")).IsSuccess());Stage=1;return false;}
  if(Stage==1){Trigger=true;const auto R=Call(TEXT("SelectAnchors"));T->TestTrue(TEXT("真实同步选择事件触发Stop"),StoppedInside);T->TestFalse(TEXT("旧同步栈不发布成功"),R.IsSuccess());FrozenObservations=Observations;
   T->TestFalse(TEXT("未清理旧租约禁止后继接管"),Host->AcquireEditorFixture({FGuid::NewGuid(),FGuid::NewGuid(),2},*W,TEXT("GlooPrint.format")).IsSuccess());Stage=2;return false;}
  if(Stage==2){FGamePlatformResult R;Adapter->Stop([&](const auto&,const auto& V){R=V;});if(!R.IsSuccess())return false;T->TestEqual(TEXT("失活期间无旧UI通知"),Observations,FrozenObservations);Adapter.Reset();
   if(!Prepare(*W,TEXT("GlooPrint.format"))){T->AddError(TEXT("原租约释放后不能重进"));return true;}T->TestTrue(TEXT("后继Run独立图"),Call(TEXT("CreateGraph")).IsSuccess());Stage=3;return false;}
  if(Stage==3){T->TestTrue(TEXT("真实Gloo任务受理"),Call(TEXT("QueueFormat")).IsSuccess());T->TestTrue(TEXT("取消只针对本Request"),Call(TEXT("CancelFormat")).IsSuccess());FrozenObservations=Observations;
   ExitedWorld=W;ExitedGI=W->GetGameInstance();ExitedWidget=Widget.Get();ExitedHost=Host.Get();
   T->TestTrue(TEXT("退出前四个弱观察均指向本次真实PIE对象"),ExitedWorld.IsValid()&&ExitedGI.IsValid()&&ExitedWidget.IsValid()&&ExitedHost.IsValid());
   // EndPlayMap会在下一次latent Update前执行GC；测试不得强持Outer属于退役GI的对象。
   // Adapter保留到退出后，继续由真实OnWorldCleanup清理，再验证迟到Stop和冻结观察。
   Widget.Reset();Host.Reset();
   GEditor->RequestEndPlayMap();Stage=4;return false;}
  if(Stage==4){if(W)return false;
   // 按对象索引/序号检查实际回收；不能仅因Garbage/Unreachable标志失效就算GC通过。
   T->TestFalse(TEXT("EndPIE已回收原World"),ExitedWorld.IsValid(true,true));
   T->TestFalse(TEXT("EndPIE已回收原GameInstance"),ExitedGI.IsValid(true,true));
   T->TestFalse(TEXT("EndPIE已回收原样板Widget"),ExitedWidget.IsValid(true,true));
   T->TestFalse(TEXT("EndPIE已回收原ManualHost"),ExitedHost.IsValid(true,true));
   FGamePlatformResult R;Adapter->Stop([&](const auto&,const auto& V){R=V;});if(!R.IsSuccess())return false;T->TestEqual(TEXT("World退出/迟到终态不能恢复UI"),Observations,FrozenObservations);T->TestFalse(TEXT("旧Run世界退出后不可执行"),Call(TEXT("Snapshot")).IsSuccess());
   auto* Resources=IGamePlatformEditorResources::Get();auto* EW=GEditor->GetEditorWorldContext().World();FGamePlatformEditorResourceLease Lease;FString Error;
   if(Resources&&EW){T->TestTrue(TEXT("退出后原Editor资源确实释放"),Resources->Acquire(*GEditor,*EW,FGuid::NewGuid(),FGuid::NewGuid(),1,Lease,Error));if(Lease.IsValid())T->TestTrue(TEXT("检查租约精确释放"),Resources->Release(Lease,Error));}else T->AddError(TEXT("缺真实Editor资源上下文"));Adapter.Reset();Widget.Reset();Host.Reset();return true;}
  return false;
 }
};
struct FPlaySettingsRestore {int32 Clients=1;bool OneProcess=true;EPlayNetMode Mode=PIE_Standalone;};
class FRestorePlaySettings final:public IAutomationLatentCommand {
 TSharedRef<FPlaySettingsRestore> Saved;FAutomationTestBase* T;
public:FRestorePlaySettings(TSharedRef<FPlaySettingsRestore> V,FAutomationTestBase& In):Saved(V),T(&In){}
 bool Update()override{if(GEditor&&GEditor->PlayWorld)return false;auto* S=GetMutableDefault<ULevelEditorPlaySettings>();int32 Clients;bool One;EPlayNetMode Mode;S->GetPlayNumberOfClients(Clients);S->GetRunUnderOneProcess(One);S->GetPlayNetMode(Mode);if(Clients!=2||!One||Mode!=PIE_Standalone){T->AddError(TEXT("外部已改PlaySettings，保留后继值并报告恢复冲突"));return true;}S->SetPlayNumberOfClients(Saved->Clients);S->SetRunUnderOneProcess(Saved->OneProcess);S->SetPlayNetMode(Saved->Mode);return true;}
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorLiveAdapters,"DivineBeasts.PluginDemos.Editor.PIE.NineVendorLifecycle",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEditorLiveAdapters::RunTest(const FString&){if(!GEditor||GEditor->PlayWorld){AddError(TEXT("要求空闲正常Editor，专属Hub和EditorFixture参数由Launcher提供"));return false;}ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FEditorAdapterLifecycle(*this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorReentryAndExit,"DivineBeasts.PluginDemos.Editor.PIE.StopReentryAndWorldExit",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEditorReentryAndExit::RunTest(const FString&){if(!GEditor||GEditor->PlayWorld){AddError(TEXT("要求闲置真实Editor"));return false;}ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FEditorStopAndExit(*this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorTwoGI,"DivineBeasts.PluginDemos.Editor.PIE.TwoEngineGameInstancesExclusive",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEditorTwoGI::RunTest(const FString&){
 if(!GEditor||GEditor->PlayWorld){AddError(TEXT("要求闲置真实Editor"));return false;}auto Saved=MakeShared<FPlaySettingsRestore>();auto* S=GetMutableDefault<ULevelEditorPlaySettings>();S->GetPlayNumberOfClients(Saved->Clients);S->GetRunUnderOneProcess(Saved->OneProcess);S->GetPlayNetMode(Saved->Mode);S->SetPlayNumberOfClients(2);S->SetRunUnderOneProcess(true);S->SetPlayNetMode(PIE_Standalone);
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));ADD_LATENT_AUTOMATION_COMMAND(FTwoPIEResourceCheck(*this));ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());ADD_LATENT_AUTOMATION_COMMAND(FRestorePlaySettings(Saved,*this));return true;
}
#endif

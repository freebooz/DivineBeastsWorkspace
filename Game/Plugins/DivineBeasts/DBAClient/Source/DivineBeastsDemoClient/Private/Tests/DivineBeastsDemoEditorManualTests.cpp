// T06开发人工页：项目私有Editor适配或平台中立租约；仅游戏线程，事件驱动。
// 只拥有本Run对象/完整租约，停止失败保留身份供重试；不操作用户图、账号或生产服务器。
#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Scenarios/DivineBeastsDemoEditorScenarios.h"
#include "Manual/GamePlatformEditorResources.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ScopedTransaction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphNode_Comment.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "Engine/Blueprint.h"
#include "BlueprintAutoLayoutManual.h"
#include "GlooPrintManual.h"
#include "UObject/StrongObjectPtr.h"
using namespace DivineBeasts::Demo;
namespace {
FDivineBeastsDemoRunToken NewEditorToken(){return {FGuid::NewGuid(),FGuid::NewGuid(),1};}
UWorld* PIEWorld(){return GEditor?GEditor->PlayWorld:nullptr;}
class FEditorLeaseCheck final:public IAutomationLatentCommand {
 FAutomationTestBase* T;double Deadline=FPlatformTime::Seconds()+40;
public:explicit FEditorLeaseCheck(FAutomationTestBase& In):T(&In){}
 bool Update()override{
  UWorld* W=PIEWorld();if(!W||!W->HasBegunPlay()){if(FPlatformTime::Seconds()>Deadline){T->AddError(TEXT("真实PIE未在40秒内就绪；不能算成功。"));return true;}return false;}
  auto* GI=W->GetGameInstance();auto* Host=GI?GI->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>():nullptr;
  if(!Host){T->AddError(TEXT("真实PIE的引擎GI缺ManualHost"));return true;}
  const auto A=NewEditorToken(),B=NewEditorToken();
  T->TestFalse(TEXT("普通Acquire仍拒绝真实PIE"),Host->Acquire(A,*W).IsSuccess());
  T->TestFalse(TEXT("Gameplay没有Editor例外"),Host->AcquireEditorFixture(A,*W,TEXT("GamePlatformGameplay.experience")).IsSuccess());
  T->TestFalse(TEXT("World没有Editor例外"),Host->AcquireEditorFixture(A,*W,TEXT("GamePlatformWorld.readiness")).IsSuccess());
  T->TestTrue(TEXT("四页DeveloperTools可由真实PIE取得租约"),Host->AcquireEditorFixture(A,*W,TEXT("GamePlatformDeveloperTools.human_review")).IsSuccess());
  auto* Resources=IGamePlatformEditorResources::Get();FGamePlatformEditorResourceLease Other;
  FString Error;
  T->TestTrue(TEXT("租约属于现有Editor模块"),Resources!=nullptr);
  if(Resources)T->TestFalse(TEXT("另一个World/Run不能取得全局Editor资源"),Resources->Acquire(*GI,*W,B.RunId,B.WorldId,B.Generation,Other,Error));
  T->TestFalse(TEXT("旧token不能释放后继"),Host->Release(B).IsSuccess());
  T->TestTrue(TEXT("原token精确释放"),Host->Release(A).IsSuccess());
  T->TestTrue(TEXT("释放后可重新进入"),Host->AcquireEditorFixture(B,*W,TEXT("BlueprintAutoLayout.whole")).IsSuccess());
  T->TestTrue(TEXT("后继释放"),Host->Release(B).IsSuccess());return true;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorManualHostPIE,"DivineBeasts.PluginDemos.Editor.PIE.HostEligibilityAndLease",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEditorManualHostPIE::RunTest(const FString&){
 if(!GEditor||GEditor->PlayWorld||!FParse::Param(FCommandLine::Get(),TEXT("GamePlatformManualEditorFixture"))){AddError(TEXT("要求正常Editor、专属Hub、显式EditorFixture且尚未PIE；测试通过引擎Play Session创建PIE"));return false;}
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FEditorLeaseCheck(*this));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEditorManualContracts,"DivineBeasts.PluginDemos.Editor.ContractClosed",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FEditorManualContracts::RunTest(const FString&){
 const auto Features=EditorManualFeatures();TestEqual(TEXT("确切九组第三方能力"),Features.Num(),9);
 for(FName F:Features){auto C=EditorManualContract(F);TestTrue(TEXT("作者合同与实现同源"),ValidateEditorManualContract(C));C.Operations.Add(TEXT("ExecText"));TestFalse(TEXT("拒绝任意命令"),ValidateEditorManualContract(C));}
 TestFalse(TEXT("普通World不在Editor名单"),UDivineBeastsDemoManualHostSubsystem::IsEditorFeature(TEXT("GamePlatformWorld.readiness")));
 TestFalse(TEXT("近似前缀不授权"),UDivineBeastsDemoManualHostSubsystem::IsEditorFeature(TEXT("BlueprintAutoLayout.whole.Other")));return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVendorManualOwnedGraph,"DivineBeasts.PluginDemos.Editor.Vendor.OwnedGraphAndConflict",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FVendorManualOwnedGraph::RunTest(const FString&){
 if(!GEditor||GEditor->IsTransactionActive()){AddError(TEXT("真实Editor闲置事务前提不满足"));return false;}
 TStrongObjectPtr<UBlueprint> BP(FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),TEXT("DBAManualVendorNative")),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass()));
 if(!BP.IsValid()){AddError(TEXT("真实Blueprint创建失败"));return false;}
 auto* Graph=FBlueprintEditorUtils::CreateNewGraph(BP.Get(),TEXT("ManualGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());FBlueprintEditorUtils::AddUbergraphPage(BP.Get(),Graph);Graph->SetFlags(RF_Transactional);
 for(int32 I=0;I<2;++I){auto* Event=NewObject<UK2Node_CustomEvent>(Graph,NAME_None,RF_Transactional);Graph->AddNode(Event);Event->CreateNewGuid();Event->CustomFunctionName=FName(*FString::Printf(TEXT("ManualEvent%d"),I));Event->AllocateDefaultPins();Event->NodePosX=I*23;Event->NodePosY=I*7;auto* Branch=NewObject<UK2Node_IfThenElse>(Graph,NAME_None,RF_Transactional);Graph->AddNode(Branch);Branch->CreateNewGuid();Branch->AllocateDefaultPins();Branch->NodePosX=450-I*17;Branch->NodePosY=I*19;Graph->GetSchema()->TryCreateConnection(Event->FindPinChecked(UEdGraphSchema_K2::PN_Then),Branch->GetExecPin());}
 FString Error;TArray<UEdGraphNode*> Empty;
 auto* InvalidGraph=NewObject<UEdGraph>(BP.Get());TestFalse(TEXT("没有合格Schema的图必须拒绝"),BlueprintAutoLayoutManual::Execute(*InvalidGraph,EBlueprintAutoLayoutManualAction::Whole,Empty,Error));
 auto* InvalidNode=NewObject<UEdGraphNode>(Graph);InvalidNode->MarkAsGarbage();TArray<UEdGraphNode*> InvalidSelection{InvalidNode};TestFalse(TEXT("销毁节点明确拒绝"),BlueprintAutoLayoutManual::Execute(*Graph,EBlueprintAutoLayoutManualAction::Selected,InvalidSelection,Error));
 TestFalse(TEXT("供应商选择桥拒绝无选择"),BlueprintAutoLayoutManual::Execute(*Graph,EBlueprintAutoLayoutManualAction::Selected,Empty,Error));
 TestTrue(TEXT("真实供应商全图操作"),BlueprintAutoLayoutManual::Execute(*Graph,EBlueprintAutoLayoutManualAction::Whole,Empty,Error));
 TestTrue(TEXT("真实供应商注释分组"),BlueprintAutoLayoutManual::Execute(*Graph,EBlueprintAutoLayoutManualAction::Group,Empty,Error));
 TestTrue(TEXT("供应商确实生成分组注释"),Graph->Nodes.ContainsByPredicate([](const UEdGraphNode* N){return N&&N->IsA<UEdGraphNode_Comment>();}));
 TestTrue(TEXT("真实供应商路由"),BlueprintAutoLayoutManual::Execute(*Graph,EBlueprintAutoLayoutManualAction::Route,Empty,Error));
 TestTrue(TEXT("真实供应商分组路由"),BlueprintAutoLayoutManual::Execute(*Graph,EBlueprintAutoLayoutManualAction::GroupRoute,Empty,Error));
 BlueprintAutoLayoutManual::FSettingsSnapshot Original,Expected;BlueprintAutoLayoutManual::ReadSettings(Original);Expected=Original;Expected.HorizontalSpacing=221;
 TestTrue(TEXT("设置通过owner窄桥写入"),BlueprintAutoLayoutManual::ApplySettings(Expected,Error));auto External=Expected;External.HorizontalSpacing=222;BlueprintAutoLayoutManual::ApplySettings(External,Error);
 TestFalse(TEXT("外部设置后继不可被Stop覆盖"),BlueprintAutoLayoutManual::RestoreSettings(Original,Expected,Error));BlueprintAutoLayoutManual::FSettingsSnapshot Current;BlueprintAutoLayoutManual::ReadSettings(Current);TestEqual(TEXT("外部值保留"),Current.HorizontalSpacing,222);
 // 外部变化夹具也由本测试拥有；先断言精确当前快照，再恢复本测试全部改动。
 TestTrue(TEXT("精确当前快照允许恢复测试所有修改"),BlueprintAutoLayoutManual::RestoreSettings(Original,External,Error));
 GlooPrintManual::FSettingsSnapshot GO,GE;GlooPrintManual::ReadSettings(GO);GE=GO;GE.HorizontalSpacing=173;TestTrue(TEXT("真实GlooPrint设置通知"),GlooPrintManual::ApplySettings(GE,Error));auto GX=GE;GX.VerticalSpacing=59;GlooPrintManual::ApplySettings(GX,Error);TestFalse(TEXT("Gloo外部后继不被恢复覆盖"),GlooPrintManual::RestoreSettings(GO,GE,Error));TestTrue(TEXT("精确GlooPrint恢复"),GlooPrintManual::RestoreSettings(GO,GX,Error));
 return true;
}
#endif

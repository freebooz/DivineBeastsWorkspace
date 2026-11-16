// T06开发人工页：项目私有Editor适配或平台中立租约；仅游戏线程，事件驱动。
// 只拥有本Run对象/完整租约，停止失败保留身份供重试；不操作用户图、账号或生产服务器。
#include "Scenarios/DivineBeastsDemoEditorScenarios.h"
#include "Scenarios/DivineBeastsDemoManualHostSubsystem.h"
#include "Scenarios/DivineBeastsDemoPlatformManualBase.h"
#include "Blueprint/UserWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_EDITOR
#include "BlueprintAutoLayoutManual.h"
#include "GlooPrintManual.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_IfThenElse.h"
#include "GraphEditor.h"
#include "SGraphPanel.h"
#include "Framework/Application/SlateApplication.h"

#include "Widgets/SWindow.h"
#include "MaterialGraph/MaterialGraph.h"
#include "MaterialGraph/MaterialGraphSchema.h"
#include "MaterialGraph/MaterialGraphNode.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "MaterialEditingLibrary.h"
#endif
namespace DivineBeasts::Demo {
namespace {
FGamePlatformResult EditorError(const TCHAR* C,const FString& M){return FGamePlatformResult::Failure(C,M);}
struct FEditorManualState:public TSharedFromThis<FEditorManualState> {
 FDivineBeastsDemoScenarioContext Context;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<ULocalPlayer> Player;TStrongObjectPtr<UDivineBeastsDemoManualHostSubsystem> Host;
 FName Feature;
#if WITH_EDITOR
 FGamePlatformEditorResourceLease EditorCleanupLease;bool bPreserveExternalOnRetry=false;FGamePlatformEditorResourceLease RecoveryLease;FGamePlatformEditorCleanupSnapshot RecoverySnapshot;FDelegateHandle RecoveryChanged;
#endif
 bool bRecoveryOnly=false;bool bBound=false,bActive=false,bHost=false,bCleaning=false,bCallingVendor=false,bWorldExiting=false;uint64 Epoch=1;FDelegateHandle WorldCleanup,SettingsChanged,GlooSettingsChanged,LayoutSettingsChanged;
#if WITH_EDITOR
 TStrongObjectPtr<UBlueprint> Blueprint;TStrongObjectPtr<UMaterial> Material;TWeakObjectPtr<UEdGraph> Graph;TSharedPtr<SGraphEditor> GraphEditor;TSharedPtr<SWindow> Window;
 BlueprintAutoLayoutManual::FSettingsSnapshot OriginalLayout,ExpectedLayout;GlooPrintManual::FSettingsSnapshot OriginalGloo,ExpectedGloo;
 bool bLayoutCaptured=false,bGlooCaptured=false,bLayoutWritten=false,bGlooWritten=false,bWindowClosed=false,bWindowCloseRequested=false;FDelegateHandle GraphChanged;
 TArray<FGuid> OwnedUndo;GlooPrintManual::FOwnedGraph QualifiedGraph;GlooPrintManual::FRequest Request;bool bRequestPending=false;FString LastTerminal=TEXT("未提交"),LastReason;int32 ChangedNodes=0;
#endif
 bool Alive()const{return bActive&&!bCleaning&&World.IsValid()&&!World->bIsTearingDown&&Player.IsValid()&&Player->GetWorld()==World.Get()&&Host.IsValid()&&(bRecoveryOnly?Host->ValidateEditorFixtureSource(Context.Token,*World,Feature).IsSuccess():Host->OwnsEditorFixture(Context.Token));}
 bool Current(uint64 E)const{return Alive()&&Epoch==E;}
 void Observe(FString Text){if(Alive()&&Context.PublishObservation)Context.PublishObservation(Context.Token,Text.Left(4096));}
 void Snapshot(){
#if WITH_EDITOR

  if(bRecoveryOnly){Observe(FString::Printf(TEXT("Editor恢复只读模式 Run=%s Pending=%d PendingLease=%s OriginalRun=%s OriginalWorld=%s Generation=%llu Error=%s；未取得/接管原资源。InspectPendingCleanup观察，RetryPendingCleanup重试，PreserveExternalAndRetry明确保留外部设置；成功后Stop再Prepare。"),*Context.Token.RunId.ToString(),RecoverySnapshot.bPending,*RecoveryLease.LeaseId.ToString(),*RecoveryLease.RunId.ToString(),*RecoveryLease.WorldId.ToString(),RecoveryLease.Generation,*RecoverySnapshot.LastError));return;}
  FString Settings;
  if(bLayoutCaptured){BlueprintAutoLayoutManual::FSettingsSnapshot V;BlueprintAutoLayoutManual::ReadSettings(V);Settings=FString::Printf(TEXT(" Layout spacing=%d/%d branch=%d event=%d wire=%d color=%d layered=%d long=%d straighten=%d"),V.HorizontalSpacing,V.VerticalSpacing,V.BranchSpacing,V.EventSpacing,V.WireHandling,V.CommentColor,V.bLayered,V.bLongEdges,V.Straighten);}
  if(bGlooCaptured){GlooPrintManual::FSettingsSnapshot V;GlooPrintManual::ReadSettings(V);Settings=FString::Printf(TEXT(" Gloo enabled=%d wire=%d spacing=%.1f/%.1f padding=%.1f"),V.bEnabled,V.WireStyle,V.HorizontalSpacing,V.VerticalSpacing,V.CommentPadding);}
  Observe(FString::Printf(TEXT("真实Editor Run=%s Graph=%s Nodes=%d Selection=%d Pending=%d 终态=%s Request=%s Changed=%d；受理/供应商终态/图像/快捷键人工验收分别记录。%s 原因=%s"),*Context.Token.RunId.ToString(),*GetNameSafe(Graph.Get()),Graph.IsValid()?Graph->Nodes.Num():0,GraphEditor?GraphEditor->GetSelectedNodes().Num():0,bRequestPending,*LastTerminal,*Request.Id.ToString(),ChangedNodes,*Settings,*LastReason));
#endif
 }
 FGamePlatformResult Prepare(){
#if WITH_EDITOR
  auto* Widget=Cast<UUserWidget>(Context.SampleWidget.Get());Player=Widget?Widget->GetOwningLocalPlayer():nullptr;
  auto* GI=World.IsValid()?World->GetGameInstance():nullptr;
  if(!GI||!Player.IsValid()||Player->GetWorld()!=World.Get())return EditorError(TEXT("Demo.Editor.Source"),TEXT("需要引擎PIE的同World/LP样板控件。"));
  Host.Reset(GI->GetSubsystem<UDivineBeastsDemoManualHostSubsystem>());if(!Host.IsValid())return EditorError(TEXT("Demo.Editor.Host"),TEXT("真实GI没有ManualHost。"));

  const auto Eligibility=Host->ValidateEditorFixtureSource(Context.Token,*World,Feature);if(!Eligibility.IsSuccess())return Eligibility;
  if(auto* Resources=IGamePlatformEditorResources::Get()){
   const auto Pending=Resources->GetCleanupSnapshot();
   if(Pending.bPending){
    // 新中央runner可显示/处置旧清理，但从不Acquire旧租约或创建新图、修改设置。
    bRecoveryOnly=true;RecoveryLease=Pending.Lease;RecoverySnapshot=Pending;bActive=true;const auto Weak=TWeakPtr<FEditorManualState>(AsShared());
    WorldCleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Weak](UWorld* W,bool,bool){if(auto S=Weak.Pin())if(S->World.Get()==W){S->bActive=false;++S->Epoch;S->Cleanup();}});
    RecoveryChanged=Resources->AddCleanupChanged(FGamePlatformEditorCleanupChanged::FDelegate::CreateLambda([Weak](const auto& V){if(auto S=Weak.Pin())if(S->Alive()){S->RecoverySnapshot=V;S->Snapshot();}}));
    const auto E=Epoch;Snapshot();return Current(E)?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("恢复页首次快照期间来源失活。"));
   }
  }
  auto R=Host->AcquireEditorFixture(Context.Token,*World,Feature);if(!R.IsSuccess())return R;bHost=true;EditorCleanupLease=Host->CopyEditorLease(Context.Token);
  // 只捕获本功能所属供应商的真实设置；未写入的值不恢复，键位只读取不改写。
  if(Feature.ToString().StartsWith(TEXT("BlueprintAutoLayout."))){BlueprintAutoLayoutManual::ReadSettings(OriginalLayout);ExpectedLayout=OriginalLayout;bLayoutCaptured=true;}
  else{GlooPrintManual::ReadSettings(OriginalGloo);ExpectedGloo=OriginalGloo;bGlooCaptured=true;}
  bActive=true;const auto Weak=TWeakPtr<FEditorManualState>(AsShared());
  WorldCleanup=FWorldDelegates::OnWorldCleanup.AddLambda([Weak](UWorld* W,bool,bool){if(auto S=Weak.Pin())if(S->World.Get()==W){S->bWorldExiting=true;S->bActive=false;++S->Epoch;S->TransferCleanupIfFailed(S->Cleanup());}});

  SettingsChanged=FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda([Weak](UObject* Object,FPropertyChangedEvent&){if(auto S=Weak.Pin())if(S->Alive()&&Object){const FName Class=Object->GetClass()->GetFName();if((S->bLayoutCaptured&&Class==TEXT("BlueprintAutoLayoutSettings"))||(S->bGlooCaptured&&Class==TEXT("GlooPrintSettings")))S->Snapshot();}});
  if(bLayoutCaptured)LayoutSettingsChanged=BlueprintAutoLayoutManual::AddSettingsChanged(FSimpleDelegate::CreateLambda([Weak]{if(auto S=Weak.Pin())if(S->Alive())S->Snapshot();}));
  if(bGlooCaptured)GlooSettingsChanged=GlooPrintManual::AddSettingsChanged(FSimpleDelegate::CreateLambda([Weak]{if(auto S=Weak.Pin())if(S->Alive())S->Snapshot();}));
  const auto E=Epoch;Snapshot();return Current(E)?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("首次快照期间已停止。"));
#else
  return FGamePlatformResult::Unsupported(TEXT("Demo.EditorHostRequired"),TEXT("本页功能需要真正Editor PIE；普通Client不链接Editor或第三方Editor模块。"));
#endif
 }
#if WITH_EDITOR
 bool IsOwnedTransaction(FGuid Id)const {
  if(!GEditor||!GEditor->Trans||!Id.IsValid()||(!Blueprint.IsValid()&&!Material.IsValid())||!Graph.IsValid())return false;
  const int32 I=GEditor->Trans->FindTransactionIndex(Id);const auto* T=I==INDEX_NONE?nullptr:GEditor->Trans->GetTransaction(I);if(!T)return false;
  TArray<UObject*> Objects;T->GetTransactionObjects(Objects);if(Objects.IsEmpty())return false;
  UObject* Root=Blueprint.IsValid()?static_cast<UObject*>(Blueprint.Get()):static_cast<UObject*>(Material.Get());
  bool bGraph=false;for(auto* O:Objects){if(!IsValid(O)||!(O==Root||O->IsIn(Root)))return false;bGraph|=O==Graph.Get()||O->IsIn(Graph.Get());}return bGraph;
 }
 FGuid UndoId()const{return GEditor&&GEditor->Trans&&GEditor->Trans->CanUndo()?GEditor->Trans->GetUndoContext().TransactionId:FGuid();}
 void CaptureUndo(FGuid Before){const auto Id=UndoId();if(Id!=Before&&IsOwnedTransaction(Id)&&(OwnedUndo.IsEmpty()||OwnedUndo.Last()!=Id))OwnedUndo.Add(Id);}
 FGamePlatformResult Undo(){
  if(bRequestPending||bCallingVendor)return EditorError(TEXT("Demo.Editor.WorkInFlight"),TEXT("等待或取消本Run原生任务后才能Undo。"));
  if(OwnedUndo.IsEmpty()||!GEditor||!GEditor->Trans||GEditor->IsTransactionActive()||UndoId()!=OwnedUndo.Last()||!IsOwnedTransaction(OwnedUndo.Last()))return EditorError(TEXT("Demo.Editor.UndoConflict"),TEXT("当前可撤销事务不属于本Run；拒绝撤销用户后继事务。"));
  TGuardValue<bool> Calling(bCallingVendor,true);if(!GEditor->Trans->Undo())return EditorError(TEXT("Demo.Editor.UndoFailed"),TEXT("真实事务系统拒绝Undo。"));OwnedUndo.Pop();return FGamePlatformResult::Success();
 }
 FGamePlatformResult CreateGraph(int32 Kind){
  if(Blueprint.IsValid()||Material.IsValid()||Window)return EditorError(TEXT("Demo.Editor.GraphOwned"),TEXT("已有本Run图；先Reset。"));
  if(!FSlateApplication::IsInitialized()||!GEditor||GEditor->IsTransactionActive())return EditorError(TEXT("Demo.Editor.Slate"),TEXT("需要真实Slate和闲置Editor事务。"));
  const auto E=Epoch;TGuardValue<bool> Calling(bCallingVendor,true);
  UEdGraph* G=nullptr;
  if(Kind==0){Blueprint.Reset(FKismetEditorUtilities::CreateBlueprint(UObject::StaticClass(),GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UBlueprint::StaticClass(),FName(*(TEXT("DBAManualGraph_")+Context.Token.RunId.ToString(EGuidFormats::Digits)))),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass()));
  if(Blueprint.IsValid())Blueprint->SetFlags(RF_Transient|RF_Transactional);
  if(!Blueprint.IsValid())return EditorError(TEXT("Demo.Editor.Blueprint"),TEXT("真实Blueprint创建失败。"));
  if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("Blueprint创建通知期间来源失活，保留候选直到Stop重试。"));
  G=FBlueprintEditorUtils::CreateNewGraph(Blueprint.Get(),TEXT("ManualGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());FBlueprintEditorUtils::AddUbergraphPage(Blueprint.Get(),G);Graph=G;G->SetFlags(RF_Transactional);
  for(int32 I=0;I<2;++I){auto* Event=NewObject<UK2Node_CustomEvent>(G,NAME_None,RF_Transactional);G->AddNode(Event);Event->CreateNewGuid();Event->CustomFunctionName=FName(*FString::Printf(TEXT("ManualEvent%d"),I));Event->AllocateDefaultPins();Event->NodePosX=I*23;Event->NodePosY=I*9;auto* Branch=NewObject<UK2Node_IfThenElse>(G,NAME_None,RF_Transactional);G->AddNode(Branch);Branch->CreateNewGuid();Branch->AllocateDefaultPins();Branch->NodePosX=420-I*13;Branch->NodePosY=I*27;G->GetSchema()->TryCreateConnection(Event->FindPinChecked(UEdGraphSchema_K2::PN_Then),Branch->GetExecPin());}

  }else{
   Material.Reset(NewObject<UMaterial>(GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UMaterial::StaticClass(),FName(*(TEXT("DBAManualMaterial_")+Context.Token.RunId.ToString(EGuidFormats::Digits)))),RF_Transient|RF_Transactional));
   auto* MG=NewObject<UMaterialGraph>(Material.Get(),NAME_None,RF_Transient|RF_Transactional);MG->Material=Material.Get();MG->Schema=UMaterialGraphSchema::StaticClass();Material->MaterialGraph=MG;
   auto* A=UMaterialEditingLibrary::CreateMaterialExpression(Material.Get(),UMaterialExpressionConstant::StaticClass(),-123,34);auto* B=UMaterialEditingLibrary::CreateMaterialExpression(Material.Get(),UMaterialExpressionConstant::StaticClass(),-177,91);auto* M=UMaterialEditingLibrary::CreateMaterialExpression(Material.Get(),UMaterialExpressionMultiply::StaticClass(),-430,12);
   if(!A||!B||!M||!UMaterialEditingLibrary::ConnectMaterialExpressions(A,TEXT(""),M,TEXT("A"))||!UMaterialEditingLibrary::ConnectMaterialExpressions(B,TEXT(""),M,TEXT("B"))||!UMaterialEditingLibrary::ConnectMaterialProperty(M,TEXT(""),MP_BaseColor))return EditorError(TEXT("Demo.Editor.MaterialFixture"),TEXT("原生Material表达式/连线夹具创建失败。"));
   MG->RebuildGraph();G=MG;Graph=G;for(UEdGraphNode* N:G->Nodes)if(N)N->SetFlags(RF_Transactional);
  }
  if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("图创建通知期间来源失活，保留本Run候选。"));
  const auto Weak=TWeakPtr<FEditorManualState>(AsShared());

  if(bGlooCaptured){UObject* Root=Blueprint.IsValid()?static_cast<UObject*>(Blueprint.Get()):static_cast<UObject*>(Material.Get());FString Error;const auto Token=Context.Token;const auto Source=World;
   if(!GlooPrintManual::QualifyOwnedPIEGraph(*Root,*G,*World,Token.RunId,Token.WorldId,Token.Generation,[Weak,E,Token,Source]{auto S=Weak.Pin();return S&&S->Current(E)&&S->Context.Token==Token&&S->World==Source;},QualifiedGraph,Error))return EditorError(TEXT("Demo.Editor.GraphQualification"),Error);
  }
  SGraphEditor::FGraphEditorEvents Events;Events.OnSelectionChanged=SGraphEditor::FOnSelectionChanged::CreateLambda([Weak,E](const FGraphPanelSelectionSet&){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();});
  GraphEditor=SNew(SGraphEditor).GraphToEdit(G).IsEditable(true).GraphEvents(Events);
  bWindowClosed=false;bWindowCloseRequested=false;Window=SNew(SWindow).Title(FText::FromString(TEXT("SSLM 本Run图样板（原生GraphEditor）"))).ClientSize(FVector2D(1000,650))[GraphEditor.ToSharedRef()];
  
  Window->SetOnWindowClosed(FOnWindowClosed::CreateLambda([Weak,E](const TSharedRef<SWindow>& Closed){if(auto S=Weak.Pin())if(S->Window==Closed){S->bWindowClosed=true;if(S->Current(E)){S->LastTerminal=TEXT("图样板窗口已关闭；操作需先Reset再CreateGraph。");S->Snapshot();}}}));
  FSlateApplication::Get().AddWindow(Window.ToSharedRef());
  GraphChanged=G->AddOnGraphChangedHandler(FOnGraphChanged::FDelegate::CreateLambda([Weak,E](const FEdGraphEditAction&){if(auto S=Weak.Pin())if(S->Current(E))S->Snapshot();}));
  return Current(E)?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("图样板发布期间已停止。"));
 }
 FGamePlatformResult Reset(){
  if(bCallingVendor)return EditorError(TEXT("Demo.Editor.CallInFlight"),TEXT("原生同步栈未返回；保留图/窗口/设置和Editor租约供Stop重试。"));
  TGuardValue<bool> Calling(bCallingVendor,true);FString Error;
  if(Request.IsValid()){if(!GlooPrintManual::Cancel(Request,Error))return EditorError(TEXT("Demo.Editor.CancelPending"),Error);Request={};bRequestPending=false;}
  if(QualifiedGraph.IsValid()){if(!GlooPrintManual::ReleaseOwnedPIEGraph(QualifiedGraph,Error))return EditorError(TEXT("Demo.Editor.GraphQualificationPending"),Error);QualifiedGraph={};}

  if(bLayoutWritten){BlueprintAutoLayoutManual::FSettingsSnapshot Current;BlueprintAutoLayoutManual::ReadSettings(Current);
   if(!(Current==ExpectedLayout)){if(!bPreserveExternalOnRetry)return EditorError(TEXT("Demo.Editor.SettingsConflict"),TEXT("布局设置存在外部后继；需要Editor完整身份重试或明确保留外部状态。"));}
   else if(!BlueprintAutoLayoutManual::RestoreSettings(OriginalLayout,ExpectedLayout,Error))return EditorError(TEXT("Demo.Editor.SettingsRestoreFailed"),Error);
   bLayoutWritten=false;ExpectedLayout=OriginalLayout;
  }

  if(bGlooWritten){GlooPrintManual::FSettingsSnapshot Current;GlooPrintManual::ReadSettings(Current);
   if(!(Current==ExpectedGloo)){if(!bPreserveExternalOnRetry)return EditorError(TEXT("Demo.Editor.SettingsConflict"),TEXT("Gloo设置存在外部后继；需要Editor完整身份重试或明确保留外部状态。"));}
   else if(!GlooPrintManual::RestoreSettings(OriginalGloo,ExpectedGloo,Error))return EditorError(TEXT("Demo.Editor.SettingsRestoreFailed"),Error);
   bGlooWritten=false;ExpectedGloo=OriginalGloo;
  }
  if(Graph.IsValid()&&GraphChanged.IsValid())Graph->RemoveOnGraphChangedHandler(GraphChanged);GraphChanged.Reset();
  
  if(Window){if(!bWindowCloseRequested&&!bWindowClosed){bWindowCloseRequested=true;if(bWorldExiting&&FSlateApplication::IsInitialized())FSlateApplication::Get().DestroyWindowImmediately(Window.ToSharedRef());else Window->RequestDestroyWindow();}if(!bWindowClosed)return EditorError(TEXT("Demo.Editor.WindowClosePending"),TEXT("等待本Run窗口原生Closed事件；保留对象和Editor租约供Stop重试。"));Window->SetOnWindowClosed(FOnWindowClosed());Window.Reset();}GraphEditor.Reset();Graph.Reset();Blueprint.Reset();Material.Reset();OwnedUndo.Reset();
  return FGamePlatformResult::Success();
 }
 FGamePlatformResult Operation(FName O,const FManualArguments& A){

  if(bCallingVendor)return EditorError(TEXT("Demo.Editor.OperationReentry"),TEXT("原生同步栈未返回，拒绝重入操作；Stop将保留资源供重试。"));
  if(O==TEXT("InspectPendingCleanup")){if(auto* R=IGamePlatformEditorResources::Get()){if(bRecoveryOnly)RecoverySnapshot=R->GetCleanupSnapshot();Snapshot();return Alive()?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("恢复观察期间已失活。"));}return EditorError(TEXT("Demo.Editor.RecoveryServiceMissing"),TEXT("原Editor资源服务不可用。"));}
  if(O==TEXT("RetryPendingCleanup")||O==TEXT("PreserveExternalAndRetry")){
   auto* Resources=IGamePlatformEditorResources::Get();if(!bRecoveryOnly||!Resources||!RecoverySnapshot.bPending||!(RecoverySnapshot.Lease==RecoveryLease))return EditorError(TEXT("Demo.Editor.NoMatchingPendingCleanup"),TEXT("没有本恢复页捕获的完整挂起身份；不能处置后继租约。"));
   const auto E=Epoch;TGuardValue<bool> Calling(bCallingVendor,true);FString Error;const bool Done=Resources->RetryCleanup(RecoveryLease,O==TEXT("RetryPendingCleanup")?EGamePlatformEditorCleanupPolicy::RetryUnchanged:EGamePlatformEditorCleanupPolicy::PreserveExternalChanges,Error);
   if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("恢复通知期间当前页面来源失活。"));return Done?FGamePlatformResult::Success():EditorError(TEXT("Demo.Editor.RecoveryPending"),Error);
  }
  if(bRecoveryOnly)return EditorError(TEXT("Demo.Editor.RecoveryReadOnly"),TEXT("挂起清理恢复模式仅允许观察/精确重试/保留外部设置/Stop；成功后重新Prepare才能操作新图。"));
  if(O==TEXT("Reset")){++Epoch;const auto R=Reset();return R;}
  if(O==TEXT("CreateGraph"))return CreateGraph(bGlooCaptured?A.Int(TEXT("GraphKind")):0);
  if(O==TEXT("InspectShortcuts")){FString V;const bool B=bLayoutCaptured?BlueprintAutoLayoutManual::InspectShortcuts(V):GlooPrintManual::InspectShortcuts(V);if(!B)return EditorError(TEXT("Demo.Editor.ShortcutRegistration"),TEXT("真实供应商命令/输入处理器注册不完整。"));const auto E=Epoch;Observe(V);return Current(E)?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("快捷键快照期间已停止。"));}
  if(O==TEXT("ApplySettings")){
   FString Error;const auto E=Epoch;TGuardValue<bool> Calling(bCallingVendor,true);
   if(bLayoutCaptured){BlueprintAutoLayoutManual::FSettingsSnapshot Current;BlueprintAutoLayoutManual::ReadSettings(Current);if(!(Current==ExpectedLayout))return EditorError(TEXT("Demo.Editor.SettingsConflict"),TEXT("布局设置已被外部修改。"));auto V=ExpectedLayout;V.HorizontalSpacing=A.Int(TEXT("HorizontalSpacing"));V.VerticalSpacing=A.Int(TEXT("VerticalSpacing"));V.WireHandling=A.Int(TEXT("WireHandling"));V.CommentColor=A.Int(TEXT("CommentColor"));V.bLayered=A.Int(TEXT("Layered"))!=0;V.bLongEdges=A.Int(TEXT("LongEdges"))!=0;V.Straighten=A.Int(TEXT("Straighten"));V.BranchSpacing=A.Int(TEXT("BranchSpacing"));V.EventSpacing=A.Int(TEXT("EventSpacing"));ExpectedLayout=V;bLayoutWritten=true;if(!BlueprintAutoLayoutManual::ApplySettings(V,Error))return EditorError(TEXT("Demo.Editor.Settings"),Error);}
   else{GlooPrintManual::FSettingsSnapshot Current;GlooPrintManual::ReadSettings(Current);if(!(Current==ExpectedGloo))return EditorError(TEXT("Demo.Editor.SettingsConflict"),TEXT("GlooPrint设置已被外部修改。"));auto V=ExpectedGloo;V.bEnabled=A.Int(TEXT("Enabled"))!=0;V.WireStyle=A.Int(TEXT("WireStyle"));V.HorizontalSpacing=A.Int(TEXT("HorizontalSpacing"));V.VerticalSpacing=A.Int(TEXT("VerticalSpacing"));V.CommentPadding=A.Int(TEXT("CommentPadding"));ExpectedGloo=V;bGlooWritten=true;if(!GlooPrintManual::ApplySettings(V,Error))return EditorError(TEXT("Demo.Editor.Settings"),Error);}
   return Current(E)?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("设置通知期间已停止；原设置归属保留至Stop重试。"));
  }
  if(O==TEXT("CancelFormat")){TGuardValue<bool> Calling(bCallingVendor,true);FString Error;if(!Request.IsValid())return EditorError(TEXT("Demo.Editor.NoRequest"),TEXT("没有本Run请求。"));return GlooPrintManual::Cancel(Request,Error)?FGamePlatformResult::Success():EditorError(TEXT("Demo.Editor.CancelConflict"),Error);}
  if(O==TEXT("Undo"))return Undo();
  if(!Graph.IsValid()||(!Blueprint.IsValid()&&!Material.IsValid())||!GraphEditor||!Window||!Window->IsVisible())return EditorError(TEXT("Demo.Editor.NoGraph"),TEXT("需要本Run打开且可见的真实图样板。"));
  const auto E=Epoch;
  if(bCallingVendor)return EditorError(TEXT("Demo.Editor.OperationReentry"),TEXT("原生同步操作尚未返回；拒绝重入写图。"));
  if(O==TEXT("SelectAnchors")||O==TEXT("ClearSelection")){
   TGuardValue<bool> Calling(bCallingVendor,true);GraphEditor->ClearSelectionSet();if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("选择通知期间已停止。"));if(O==TEXT("SelectAnchors")){for(UEdGraphNode* N:Graph->Nodes)if(N&&(N->IsA<UK2Node_CustomEvent>()||N->IsA<UMaterialGraphNode>())){GraphEditor->SetNodeSelection(N,true);if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("选择通知期间已停止。"));}}return FGamePlatformResult::Success();
  }
  if(O==TEXT("QueueFormat")){
   if(bRequestPending)return EditorError(TEXT("Demo.Editor.RequestBusy"),TEXT("已有本Run分片任务。"));
   const auto Weak=TWeakPtr<FEditorManualState>(AsShared());const auto Before=UndoId();const auto Token=Context.Token;const auto Source=World;TGuardValue<bool> Calling(bCallingVendor,true);
   auto Submitting=MakeShared<bool>(true);auto Terminal=MakeShared<TFunction<void()>>();FString Error;GlooPrintManual::FRequest Candidate;
   auto Finish=[Weak,E,Token,Source,Before](bool Success,bool Cancelled,int32 Changed,FString Reason){if(auto S=Weak.Pin()){
    if(S->Context.Token==Token&&S->World==Source&&S->Epoch==E){S->bRequestPending=false;S->ChangedNodes=Changed;S->LastReason=Reason.Left(512);S->LastTerminal=Cancelled?TEXT("Cancelled"):(Success?TEXT("Succeeded"):TEXT("Failed"));S->CaptureUndo(Before);if(S->Current(E))S->Snapshot();}
   }};
   const bool B=GlooPrintManual::Submit(*Graph,[Weak,E,Token,Source]{auto S=Weak.Pin();return S&&S->Context.Token==Token&&S->World==Source&&S->Current(E);},[Finish,Submitting,Terminal](bool Success,bool Cancelled,int32 Changed,const FString& Reason){if(*Submitting){*Terminal=[Finish,Success,Cancelled,Changed,Reason]{Finish(Success,Cancelled,Changed,Reason);};}else Finish(Success,Cancelled,Changed,Reason);},Candidate,Error);
   if(!B)return EditorError(TEXT("Demo.Editor.SubmitRejected"),Error);Request=Candidate;bRequestPending=true;LastTerminal=TEXT("Pending");LastReason.Reset();ChangedNodes=0;*Submitting=false;if(*Terminal){auto F=MoveTemp(*Terminal);F();}
   if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("提交或终态通知期间已停止，保留请求到Stop重试。"));
   Snapshot();if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("受理快照通知期间已停止。"));
   auto R=FGamePlatformResult::Success();R.Message=TEXT("原生任务已受理；Pending与真实终态由事件分别显示，受理不代表布局或人工验收完成。");return R;
  }
  TArray<UEdGraphNode*> Selection;for(UObject* O:GraphEditor->GetSelectedNodes())if(auto* N=Cast<UEdGraphNode>(O))Selection.Add(N);
  EBlueprintAutoLayoutManualAction Action;
  if(O==TEXT("LayoutWhole"))Action=EBlueprintAutoLayoutManualAction::Whole;else if(O==TEXT("LayoutSelected"))Action=EBlueprintAutoLayoutManualAction::Selected;else if(O==TEXT("LayoutGroup"))Action=EBlueprintAutoLayoutManualAction::Group;else if(O==TEXT("LayoutRoute"))Action=EBlueprintAutoLayoutManualAction::Route;else if(O==TEXT("LayoutGroupRoute"))Action=EBlueprintAutoLayoutManualAction::GroupRoute;else return EditorError(TEXT("Demo.Editor.Operation"),TEXT("没有此封闭操作。"));
  const auto Before=UndoId();TGuardValue<bool> Calling(bCallingVendor,true);FString Error;const bool B=BlueprintAutoLayoutManual::Execute(*Graph,Action,Selection,Error);CaptureUndo(Before);
  if(!Current(E))return FGamePlatformResult::Cancelled(TEXT("供应商同步图通知期间已停止，旧栈不发布成功。"));
  return B?FGamePlatformResult::Success():EditorError(TEXT("Demo.Editor.VendorRejected"),Error);
 }
#endif

 /** 强回调由现有Editor资源服务持有；Adapter/中央runner退役后仍有唯一精确清理状态。 */
 void TransferCleanupIfFailed(const FGamePlatformResult& Result){
#if WITH_EDITOR
  if(Result.IsSuccess()||!bHost||!EditorCleanupLease.IsValid())return;Context.PublishObservation={};auto* Resources=IGamePlatformEditorResources::Get();auto State=AsShared();
  if(!Resources||!Resources->RetainCleanup(EditorCleanupLease,[State](EGamePlatformEditorCleanupPolicy Policy,FString& Error){State->bPreserveExternalOnRetry=Policy==EGamePlatformEditorCleanupPolicy::PreserveExternalChanges;const auto R=State->Cleanup();Error=R.IsSuccess()?FString():R.Code.ToString()+TEXT(": ")+R.Message;return R.IsSuccess();},Result.Code.ToString()+TEXT(": ")+Result.Message)){UE_LOG(LogTemp,Error,TEXT("Editor原清理状态转交失败；原租约没有释放：%s"),*Result.Code.ToString());}
  else Host.Reset(); // Editor已经保留完整原租约和State；不反向强持整个退役GI来维持重试。
#endif
 }
 FGamePlatformResult Cleanup(){
  if(bCleaning)return EditorError(TEXT("Demo.Editor.CleanupReentry"),TEXT("清理重入，保留原归属供重试。"));bActive=false;++Epoch;TGuardValue<bool> Cleaning(bCleaning,true);
#if WITH_EDITOR
  if(!bRecoveryOnly){const auto R=Reset();if(!R.IsSuccess())return R;}
  if(RecoveryChanged.IsValid()){if(auto* Resources=IGamePlatformEditorResources::Get())Resources->RemoveCleanupChanged(RecoveryChanged);RecoveryChanged.Reset();}
#endif
  FWorldDelegates::OnWorldCleanup.Remove(WorldCleanup);WorldCleanup.Reset();
#if WITH_EDITOR
  FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(SettingsChanged);SettingsChanged.Reset();
  if(LayoutSettingsChanged.IsValid()){BlueprintAutoLayoutManual::RemoveSettingsChanged(LayoutSettingsChanged);LayoutSettingsChanged.Reset();}
  if(GlooSettingsChanged.IsValid()){GlooPrintManual::RemoveSettingsChanged(GlooSettingsChanged);GlooSettingsChanged.Reset();}
#endif

  if(bHost){
   if(Host.IsValid()){const auto R=Host->Release(Context.Token);if(!R.IsSuccess())return R;}
#if WITH_EDITOR
   else{auto* Resources=IGamePlatformEditorResources::Get();FString Error;if(!Resources||!Resources->Release(EditorCleanupLease,Error))return EditorError(TEXT("Demo.Editor.HostLostReleasePending"),Error);}
   EditorCleanupLease={};
#else
   else return EditorError(TEXT("Demo.Editor.HostLost"),TEXT("原宿主已失效，保留归属。"));
#endif
   bHost=false;Host.Reset();
  }
  return FGamePlatformResult::Success();
 }
};
class FEditorManualAdapter final:public IDivineBeastsDemoScenarioAdapter {
 TSharedRef<FEditorManualState> S;
public:
 FEditorManualAdapter(UWorld& W,FName F):S(MakeShared<FEditorManualState>()){S->World=&W;S->Feature=F;}
 ~FEditorManualAdapter(){const auto Result=S->Cleanup();S->TransferCleanupIfFailed(Result);}
 void Prepare(const FDivineBeastsDemoScenarioContext& C,FDivineBeastsDemoOperationCompletion D)override{auto State=S;if(State->bBound){D(C.Token,EditorError(TEXT("Demo.Editor.AlreadyBound"),TEXT("适配器不可重绑。")));return;}State->bBound=true;State->Context=C;
  if(!C.Token.IsValid()||C.World!=State->World||C.Scenario.FeatureId!=State->Feature||!ValidateEditorManualContract(C.Scenario)){D(C.Token,EditorError(TEXT("Demo.Editor.Source"),TEXT("完整Run/世界/封闭合同不符。")));return;}const auto R=State->Prepare();D(C.Token,R);
 }
 void Execute(FName O,const TArray<FDivineBeastsDemoParameterValue>& V,FDivineBeastsDemoOperationCompletion D)override{auto State=S;const auto Token=State->Context.Token;
  if(!State->Alive()){D(Token,EditorError(TEXT("Demo.Editor.Inactive"),TEXT("本Run已停止或失效。")));return;}
  const auto R=ValidateEditorManualValues(State->Context.Scenario,V);if(!R.IsSuccess()||!State->Context.Scenario.Operations.Contains(O)){D(Token,R.IsSuccess()?EditorError(TEXT("Demo.Editor.Operation"),TEXT("操作不在冻结白名单。")):R);return;}
  if(O==TEXT("Stop")){Stop(MoveTemp(D));return;}const auto E=State->Epoch;
  if(O==TEXT("Snapshot")){State->Snapshot();D(Token,State->Current(E)?FGamePlatformResult::Success():FGamePlatformResult::Cancelled(TEXT("快照通知期间已停止。")));return;}
#if WITH_EDITOR
  const auto Result=State->Operation(O,FManualArguments(State->Context,V));if(!State->Alive()){D(Token,FGamePlatformResult::Cancelled(TEXT("操作通知期间来源失活。")));return;}D(Token,Result);
#else
  D(Token,FGamePlatformResult::Unsupported(TEXT("Demo.EditorHostRequired"),TEXT("需要真实Editor。")));
#endif
 }
 void Stop(FDivineBeastsDemoOperationCompletion D)override{auto State=S;State->bActive=false;++State->Epoch;D(State->Context.Token,State->Cleanup());}
};
}
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateEditorManualScenario(UWorld& W,FName F){return EditorManualFeatures().Contains(F)?MakeUnique<FEditorManualAdapter>(W,F):nullptr;}
}

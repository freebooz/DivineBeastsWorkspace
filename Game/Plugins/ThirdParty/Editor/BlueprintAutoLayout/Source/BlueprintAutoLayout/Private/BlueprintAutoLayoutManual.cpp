#include "BlueprintAutoLayoutManual.h"
#include "BlueprintAutoLayoutModule.h"
#include "BlueprintAutoLayoutSettings.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "Modules/ModuleManager.h"
namespace BlueprintAutoLayoutManual {
FDelegateHandle AddSettingsChanged(FSimpleDelegate Handler){check(IsInGameThread());return GetMutableDefault<UBlueprintAutoLayoutSettings>()->OnSettingChanged().AddLambda([Handler=MoveTemp(Handler)](UObject*,FPropertyChangedEvent&){Handler.ExecuteIfBound();});}
void RemoveSettingsChanged(FDelegateHandle Handle){check(IsInGameThread());GetMutableDefault<UBlueprintAutoLayoutSettings>()->OnSettingChanged().Remove(Handle);}
bool Execute(UEdGraph& Graph,EBlueprintAutoLayoutManualAction A,const TArray<UEdGraphNode*>& Selection,FString& E){
 check(IsInGameThread());E.Reset();
 if(!GEditor||GEditor->IsTransactionActive()||!IsValid(&Graph)||!Graph.bEditable||!Graph.GetSchema()||!Graph.GetSchema()->IsA<UEdGraphSchema_K2>()||Graph.Nodes.IsEmpty()){E=TEXT("需要真实Editor闲置事务和可编辑非空K2图。");return false;}
 if(A==EBlueprintAutoLayoutManualAction::Selected&&Selection.IsEmpty()){E=TEXT("没有选中节点。");return false;}
 for(auto* N:Selection)if(!IsValid(N)||N->GetGraph()!=&Graph){E=TEXT("选中节点已销毁或属于其他图。");return false;}
 switch(A){
 case EBlueprintAutoLayoutManualAction::Whole:FBlueprintAutoLayoutModule::ExecuteLayoutOnGraph(&Graph);break;
 case EBlueprintAutoLayoutManualAction::Selected:FBlueprintAutoLayoutModule::ExecuteLayoutSelectedOnGraph(&Graph,Selection);break;
 case EBlueprintAutoLayoutManualAction::Group:FBlueprintAutoLayoutModule::ExecuteLayoutAndGroupOnGraph(&Graph);break;
 case EBlueprintAutoLayoutManualAction::Route:FBlueprintAutoLayoutModule::ExecuteLayoutAndRouteOnGraph(&Graph);break;
 case EBlueprintAutoLayoutManualAction::GroupRoute:FBlueprintAutoLayoutModule::ExecuteLayoutGroupAndRouteOnGraph(&Graph);break;
 default:E=TEXT("未知封闭布局操作。");return false;}
 return true;
}
void ReadSettings(FSettingsSnapshot& V){const auto* S=GetDefault<UBlueprintAutoLayoutSettings>();V.bLayered=S->bUseLayeredEngine;V.bLongEdges=S->bMaterializeLongEdges;V.WireHandling=uint8(S->WireHandling);V.CommentColor=uint8(S->CommentColorMode);V.HorizontalSpacing=S->HorizontalSpacing;V.VerticalSpacing=S->VerticalSpacing;V.BranchSpacing=S->BranchSpacing;V.EventSpacing=S->EventSpacing;V.Straighten=S->StraightenMaxNudge;}
bool ApplySettings(const FSettingsSnapshot& V,FString& E){
 check(IsInGameThread());E.Reset();if(V.WireHandling>2||V.CommentColor>1||V.HorizontalSpacing<0||V.HorizontalSpacing>1000||V.VerticalSpacing<0||V.VerticalSpacing>1000||V.BranchSpacing<0||V.BranchSpacing>1000||V.EventSpacing<0||V.EventSpacing>2000||V.Straighten<0||V.Straighten>600){E=TEXT("布局配置超出供应商原生范围。");return false;}
 auto* S=GetMutableDefault<UBlueprintAutoLayoutSettings>();S->bUseLayeredEngine=V.bLayered;S->bMaterializeLongEdges=V.bLongEdges;S->WireHandling=EBPALWireHandling(V.WireHandling);S->CommentColorMode=EBPALCommentColorMode(V.CommentColor);S->HorizontalSpacing=V.HorizontalSpacing;S->VerticalSpacing=V.VerticalSpacing;S->BranchSpacing=V.BranchSpacing;S->EventSpacing=V.EventSpacing;S->StraightenMaxNudge=V.Straighten;S->PostEditChange();return true;
}
bool RestoreSettings(const FSettingsSnapshot& O,const FSettingsSnapshot& Expected,FString& E){FSettingsSnapshot V;ReadSettings(V);if(!(V==Expected)){E=TEXT("布局设置已有外部后继修改，拒绝覆盖。");return false;}return ApplySettings(O,E);}
bool InspectShortcuts(FString& S){auto* M=FModuleManager::GetModulePtr<FBlueprintAutoLayoutModule>(TEXT("BlueprintAutoLayout"));return M&&M->InspectManualShortcuts(S);}
}

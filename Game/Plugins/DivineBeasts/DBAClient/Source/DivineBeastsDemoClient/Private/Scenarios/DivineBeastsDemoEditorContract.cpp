// T06开发人工页：项目私有Editor适配或平台中立租约；仅游戏线程，事件驱动。
// 只拥有本Run对象/完整租约，停止失败保留身份供重试；不操作用户图、账号或生产服务器。
#include "Scenarios/DivineBeastsDemoEditorScenarios.h"
namespace DivineBeasts::Demo {
TArray<FName> EditorManualFeatures(){return {TEXT("BlueprintAutoLayout.whole"),TEXT("BlueprintAutoLayout.selected"),TEXT("BlueprintAutoLayout.group"),TEXT("BlueprintAutoLayout.route"),TEXT("BlueprintAutoLayout.settings"),TEXT("GlooPrint.format"),TEXT("GlooPrint.wires"),TEXT("GlooPrint.settings"),TEXT("GlooPrint.cancelundo")};}
FDivineBeastsDemoScenarioDefinition EditorManualContract(FName F){
 FDivineBeastsDemoScenarioDefinition D;if(!EditorManualFeatures().Contains(F))return D;
 D.FeatureId=F;D.ScenarioId=FName(*(F.ToString()+TEXT(".Manual")));D.AdapterId=FName(*(TEXT("EditorManual.")+F.ToString()));
 D.Operations={TEXT("CreateGraph"),TEXT("Snapshot"),TEXT("SelectAnchors"),TEXT("ClearSelection"),TEXT("InspectShortcuts"),TEXT("Undo"),TEXT("Reset"),TEXT("Stop"),TEXT("InspectPendingCleanup"),TEXT("RetryPendingCleanup"),TEXT("PreserveExternalAndRetry")};
 auto Add=[&](const TCHAR* Name,int64 Value,int64 Min,int64 Max){FDivineBeastsDemoParameterDefinition P;P.ParameterId=Name;P.DefaultValue.Type=EDivineBeastsDemoParameterType::Integer;P.DefaultValue.IntegerValue=Value;P.Minimum=Min;P.Maximum=Max;D.Parameters.Add(P);};
 if(F==TEXT("BlueprintAutoLayout.whole"))D.Operations.Add(TEXT("LayoutWhole"));
 if(F==TEXT("BlueprintAutoLayout.selected"))D.Operations.Add(TEXT("LayoutSelected"));
 if(F==TEXT("BlueprintAutoLayout.group"))D.Operations.Add(TEXT("LayoutGroup"));
 if(F==TEXT("BlueprintAutoLayout.route")){D.Operations.Add(TEXT("LayoutRoute"));D.Operations.Add(TEXT("LayoutGroupRoute"));}
 if(F==TEXT("BlueprintAutoLayout.settings")){D.Operations.Add(TEXT("ApplySettings"));D.Operations.Add(TEXT("LayoutWhole"));Add(TEXT("HorizontalSpacing"),110,0,1000);Add(TEXT("VerticalSpacing"),44,0,1000);Add(TEXT("WireHandling"),0,0,2);Add(TEXT("CommentColor"),0,0,1);Add(TEXT("Layered"),1,0,1);Add(TEXT("LongEdges"),1,0,1);Add(TEXT("Straighten"),120,0,600);Add(TEXT("BranchSpacing"),90,0,1000);Add(TEXT("EventSpacing"),200,0,2000);}
 if(F.ToString().StartsWith(TEXT("GlooPrint."))){Add(TEXT("GraphKind"),0,0,1);D.Operations.Add(TEXT("QueueFormat"));D.Operations.Add(TEXT("CancelFormat"));}
 if(F==TEXT("GlooPrint.settings")||F==TEXT("GlooPrint.wires")){D.Operations.Add(TEXT("ApplySettings"));Add(TEXT("Enabled"),1,0,1);Add(TEXT("WireStyle"),0,0,2);Add(TEXT("HorizontalSpacing"),96,16,2048);Add(TEXT("VerticalSpacing"),48,16,2048);Add(TEXT("CommentPadding"),32,8,512);}
 return D;
}
bool ValidateEditorManualContract(const FDivineBeastsDemoScenarioDefinition& D){
 if(!EditorManualFeatures().Contains(D.FeatureId))return false;const auto E=EditorManualContract(D.FeatureId);
 if(D.AdapterId!=E.AdapterId||D.ScenarioId!=E.ScenarioId||D.Parameters.Num()!=E.Parameters.Num()||D.Operations.Num()!=E.Operations.Num())return false;
 for(int32 I=0;I<E.Parameters.Num();++I)if(D.Parameters[I].ParameterId!=E.Parameters[I].ParameterId||D.Parameters[I].DefaultValue.Type!=E.Parameters[I].DefaultValue.Type||D.Parameters[I].DefaultValue.IntegerValue!=E.Parameters[I].DefaultValue.IntegerValue||D.Parameters[I].Minimum!=E.Parameters[I].Minimum||D.Parameters[I].Maximum!=E.Parameters[I].Maximum)return false;
 TSet<FName> Seen;for(FName O:D.Operations){if(!E.Operations.Contains(O)||Seen.Contains(O))return false;Seen.Add(O);}return true;
}
FGamePlatformResult ValidateEditorManualValues(const FDivineBeastsDemoScenarioDefinition& D,const TArray<FDivineBeastsDemoParameterValue>& V){
 if(!ValidateEditorManualContract(D)||D.Parameters.Num()!=V.Num())return FGamePlatformResult::Failure(TEXT("Demo.Editor.Contract"),TEXT("封闭Editor合同或参数数量不符。"));
 for(int32 I=0;I<V.Num();++I)if(V[I].Type!=EDivineBeastsDemoParameterType::Integer||V[I].IntegerValue<D.Parameters[I].Minimum||V[I].IntegerValue>D.Parameters[I].Maximum)return FGamePlatformResult::Failure(TEXT("Demo.Editor.Parameter"),TEXT("封闭参数类型或供应商范围不符。"));
 return FGamePlatformResult::Success();
}
}

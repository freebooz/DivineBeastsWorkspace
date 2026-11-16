#pragma once
#include "CoreMinimal.h"
class UEdGraph;class UEdGraphNode;
enum class EBlueprintAutoLayoutManualAction:uint8 {Whole,Selected,Group,Route,GroupRoute};
namespace BlueprintAutoLayoutManual {
/** 封闭首选项快照；不暴露CDO所有权，不保存配置。 */
struct FSettingsSnapshot {
 bool bLayered=true,bLongEdges=true;uint8 WireHandling=0,CommentColor=0;
 int32 HorizontalSpacing=110,VerticalSpacing=44,BranchSpacing=90,EventSpacing=200,Straighten=120;
 bool operator==(const FSettingsSnapshot& B)const=default;
};
BLUEPRINTAUTOLAYOUT_API bool Execute(UEdGraph& Graph,EBlueprintAutoLayoutManualAction Action,const TArray<UEdGraphNode*>& Selection,FString& Error);
BLUEPRINTAUTOLAYOUT_API void ReadSettings(FSettingsSnapshot& Out);
BLUEPRINTAUTOLAYOUT_API bool ApplySettings(const FSettingsSnapshot& Value,FString& Error);
/** 当前值必须仍等于本调用方最后写入值；冲突保留外部值，调用方保留租约。 */
BLUEPRINTAUTOLAYOUT_API bool RestoreSettings(const FSettingsSnapshot& Original,const FSettingsSnapshot& Expected,FString& Error);
/** 检查实际TCommands注册、活跃键位和模块input processor；不把直接布局计为快捷键执行。 */
BLUEPRINTAUTOLAYOUT_API bool InspectShortcuts(FString& Snapshot);
/** 订阅UDeveloperSettings真实通知；必须配对解绑，不改键位或保存首选项。 */
BLUEPRINTAUTOLAYOUT_API FDelegateHandle AddSettingsChanged(FSimpleDelegate Handler);
BLUEPRINTAUTOLAYOUT_API void RemoveSettingsChanged(FDelegateHandle Handle);
}

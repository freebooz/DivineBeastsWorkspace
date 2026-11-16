#pragma once
#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
class UEdGraph;class UWorld;
namespace GlooPrintManual {
struct FSettingsSnapshot {bool bEnabled=true;uint8 WireStyle=0;float HorizontalSpacing=96,VerticalSpacing=48,CommentPadding=32;bool operator==(const FSettingsSnapshot& B)const=default;};

/** 只签发给真实单人PIE中显式拥有的Transient开发图；根对象/图/来源/代次不可替换。 */
struct FOwnedGraph {
 FGuid Id,RunId,WorldId;uint64 Generation=0;TWeakObjectPtr<UObject> Root;TWeakObjectPtr<UEdGraph> Graph;TWeakObjectPtr<UWorld> World;
 bool IsValid()const{return Id.IsValid()&&RunId.IsValid()&&WorldId.IsValid()&&Generation>0;}
 bool operator==(const FOwnedGraph& B)const{return Id==B.Id&&RunId==B.RunId&&WorldId==B.WorldId&&Generation==B.Generation&&Root==B.Root&&Graph==B.Graph&&World==B.World;}
};
GLOOPRINT_API bool QualifyOwnedPIEGraph(UObject& Root,UEdGraph& Graph,UWorld& World,FGuid RunId,FGuid WorldId,uint64 Generation,TFunction<bool()> Guard,FOwnedGraph& Out,FString& Error);
GLOOPRINT_API bool ReleaseOwnedPIEGraph(const FOwnedGraph& Lease,FString& Error);
/** 与捕获/规划/应用一致的真实Owner资格只读检查；普通PIE图仍被拒绝。 */
GLOOPRINT_API bool IsGraphEligible(UEdGraph& Graph,FString& Error);
struct FRequest {FGuid Id;TWeakObjectPtr<UEdGraph> Graph;bool IsValid()const{return Id.IsValid();}};
using FCompletion=TFunction<void(bool bSuccess,bool bCancelled,int32 Changed,const FString& Reason)>;
GLOOPRINT_API void ReadSettings(FSettingsSnapshot& Out);
GLOOPRINT_API bool ApplySettings(const FSettingsSnapshot& Value,FString& Error);
GLOOPRINT_API bool RestoreSettings(const FSettingsSnapshot& Original,const FSettingsSnapshot& Expected,FString& Error);
/** 借用模块已注册的唯一FEditor；原生分片任务执行，回调给真实终态。Guard由调用者核Run/World/代次。 */
GLOOPRINT_API bool Submit(UEdGraph& Graph,TFunction<bool()> Guard,FCompletion Completion,FRequest& Out,FString& Error);
/** 只取消匹配完整Request的待处理任务，不取消后继工具。没有在途任务时精确幂等。 */
GLOOPRINT_API bool Cancel(const FRequest& Request,FString& Error);
GLOOPRINT_API bool InspectShortcuts(FString& Snapshot);
/** 配对订阅供应商真实设置通知（包括ReloadConfig）；不通过UI轮询CDO。 */
GLOOPRINT_API FDelegateHandle AddSettingsChanged(FSimpleDelegate Handler);
GLOOPRINT_API void RemoveSettingsChanged(FDelegateHandle Handle);
}

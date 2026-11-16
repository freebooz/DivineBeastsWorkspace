// 平台Editor中立全局资源桥：由既有DeveloperTools模块签发精确排他租约。
// 仅游戏线程；调用者负责资格，完整句柄负责释放，不包含上层feature/vendor白名单。
#pragma once
#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"
class UWorld;
/** Editor进程内排他资源租约。发行Editor、来源对象/世界、完整调用者身份与唯一租约均须一致。 */
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformEditorResourceLease {
 FGuid LeaseId,RunId,WorldId;uint64 Generation=0;
 TWeakObjectPtr<UObject> Editor,Owner;TWeakObjectPtr<UWorld> World;
 bool IsValid()const{return LeaseId.IsValid()&&RunId.IsValid()&&WorldId.IsValid()&&Generation>0;}
 bool operator==(const FGamePlatformEditorResourceLease& B)const{return LeaseId==B.LeaseId&&RunId==B.RunId&&WorldId==B.WorldId&&Generation==B.Generation&&Editor==B.Editor&&Owner==B.Owner&&World==B.World;}
};

/** 退出后冲突处置必须显式选择；PreserveExternalChanges只放弃调用者原恢复义务，不能覆盖外部后继。 */
enum class EGamePlatformEditorCleanupPolicy:uint8 {RetryUnchanged,PreserveExternalChanges};
/** Editor作用域只读诊断；完整租约是唯一重试资格，LastError不等于清理完成。 */
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformEditorCleanupSnapshot {
 FGamePlatformEditorResourceLease Lease;bool bPending=false;FString LastError;
};
DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformEditorCleanupChanged,const FGamePlatformEditorCleanupSnapshot&);
using FGamePlatformEditorCleanupRetry=TFunction<bool(EGamePlatformEditorCleanupPolicy,FString&)>;
/** 现有DeveloperTools Editor模块的中立资源协调口；不创建第二执行器或业务单例，不持有项目资格规则。 */
class GAMEPLATFORMDEVELOPERTOOLS_API IGamePlatformEditorResources:public IModuleInterface {
public:
 static IGamePlatformEditorResources* Get();
 /** 游戏线程。非空旧租约绝不自动接管；来源失效后也须原拥有者明确释放。 */
 virtual bool Acquire(UObject& Owner,UWorld& World,FGuid RunId,FGuid WorldId,uint64 Generation,FGamePlatformEditorResourceLease& Out,FString& Error)=0;
 virtual bool Owns(const FGamePlatformEditorResourceLease& Lease)const=0;
 /** 允许来源世界退出后按完整租约释放；失败保留句柄供显式重试。 */

 virtual bool Release(const FGamePlatformEditorResourceLease& Lease,FString& Error)=0;
 /** World/GI退役后转交失败清理状态。持强回调至成功；拒绝替换/抢占另一租约。 */
 virtual bool RetainCleanup(const FGamePlatformEditorResourceLease& Lease,FGamePlatformEditorCleanupRetry Retry,const FString& Error)=0;
 /** 只在显式命令中重试；同步重入失败，失败继续持有原状态，禁止Ticker/自动强制复位。 */
 virtual bool RetryCleanup(const FGamePlatformEditorResourceLease& Lease,EGamePlatformEditorCleanupPolicy Policy,FString& Error)=0;
 virtual FGamePlatformEditorCleanupSnapshot GetCleanupSnapshot()const=0;
 /** Editor作用域成对订阅；新世界只消费完整快照/变化通知，不轮询或接管原租约。 */
 virtual FDelegateHandle AddCleanupChanged(FGamePlatformEditorCleanupChanged::FDelegate Handler)=0;
 virtual void RemoveCleanupChanged(FDelegateHandle Handle)=0;
};

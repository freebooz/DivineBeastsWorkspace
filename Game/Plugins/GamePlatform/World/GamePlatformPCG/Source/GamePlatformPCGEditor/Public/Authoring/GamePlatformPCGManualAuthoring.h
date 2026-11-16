// 平台Editor自己的窄原生桥；只转接既有创作器，不公开Private类/任意路径或执行文本。
#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformDataLease.h"
class IGamePlatformDataService;
class UGamePlatformPCGProfileDefinition;
class AGamePlatformPCGWorldDirector;
class UPCGGraph;
namespace GamePlatformPCGManualAuthoring {
 /** 游戏线程；只消费成功完整Data Definition+PCGGeneration租约，纯Resolve依赖且核对canonical路径。
  * Name是非空ASCII字母数字下划线资产标识（项目命名约束由上层检查），固定开发Realized目录，拒绝占用，不修改源Profile/模板。
  * IsCurrent在作者调用前/后核对来源；同步保存栈不能回滚，取消后SavedPath仍说明真实持久产物。
  * 调用者须在整个同步作者栈保留Data租约；Stop不会删除已保存图。
  */
 GAMEPLATFORMPCGEDITOR_API FGamePlatformResult CreateRealizedGraph(IGamePlatformDataService& Data,
  const FGamePlatformDataLease& Lease,const FSoftObjectPath& ProfilePath,const FSoftObjectPath& MeshSetPath,
  FName Name,TFunctionRef<bool()> IsCurrent,FString& SavedPath);
 /** 固定输出目录/合法标识校验，不加载、不保存；已有资产占用由作者执行前再次检查。 */
 GAMEPLATFORMPCGEDITOR_API bool MakeRealizedPackage(FName Name,FString& Package,FString& Error);
 GAMEPLATFORMPCGEDITOR_API bool CreateFoundations(FString& Error);
 GAMEPLATFORMPCGEDITOR_API bool CreateManualPlacements(FString& Error);
 GAMEPLATFORMPCGEDITOR_API bool ValidateProfile(UGamePlatformPCGProfileDefinition* Profile,FString& Error);
 GAMEPLATFORMPCGEDITOR_API bool InspectSource(UGamePlatformPCGProfileDefinition* Profile,FString& Fingerprint,TArray<FString>& Dependencies,FString& Error);
 /** 仅当前World、本Run新建未保存的Director与瞬态克隆图；不修改已保存Profile/图，不执行Bake。 */
 GAMEPLATFORMPCGEDITOR_API bool ConfigureOwnedSpatialMasks(UPCGGraph* OwnedGraph,AGamePlatformPCGWorldDirector* OwnedDirector,int32 Priority,FString& Error);
}

// 平台Editor工具：为Monolith/Python暴露引擎原生的资产依赖重映射复制，不参与任何运行时播放。
// 调用方拥有映射和迁移证据；本类不决定项目内容归属，不修改源资产或覆盖目标资产。
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GamePlatformAssetMigrationLibrary.generated.h"

/** 游戏线程显式调用；仅存在于GamePlatformDeveloperTools编辑器模块。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformAssetMigrationLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /**
     * 将完整包名映射交给AssetTools原生AdvancedCopyPackages，统一重映射硬/软引用并保存。
     * DestinationRoot必须是已挂载、非引擎的长包目录；全部目标必须位于该目录内且尚不存在。
     * 调用方先提供完整依赖闭包；缺源、目标冲突或非法路径在复制前拒绝。
     * 返回false时OutError说明拒绝/原生失败；原生复制失败可能已留下部分新资产，不能当成事务回滚。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Editor|Migration")
    static bool CopyPackagesWithReferences(const TMap<FString, FString>& PackageDestinations,
        const FString& DestinationRoot, FString& OutError);
};

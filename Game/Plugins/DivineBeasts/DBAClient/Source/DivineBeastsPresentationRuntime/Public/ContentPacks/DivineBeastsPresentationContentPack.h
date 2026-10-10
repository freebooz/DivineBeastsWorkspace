// 本文件属于DivineBeasts项目层 DivineBeastsPresentationRuntime，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 DBAClient/Docs/PresentationAuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "GamePlatformPresentationCatalog.h"
#include "DivineBeastsPresentationContentPack.generated.h"

/** FDivineBeastsPresentationContentPackHandle（项目表现内容包激活句柄）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsPresentationContentPackHandle
{
    GENERATED_BODY()

    /** 一次选择事务身份；激活受理不表示目录已经可见，客户端查询Loading/Active终态。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid Id;
    /** LocalPlayer服务身份与事务代次，拒绝跨玩家/旧服务句柄。 */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) FGuid ScopeId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly) int64 Generation = 0;
    bool IsValid() const { return Id.IsValid() && ScopeId.IsValid() && Generation > 0; }
};

/** 项目内容包事务状态；Loading时目录不可见，失败/取消不保留任何本次租约。 */
UENUM(BlueprintType)
enum class EDivineBeastsPresentationContentPackState : uint8 { Invalid, Loading, Active, Failed, Cancelled, Deactivated };

/**
 * FDivineBeastsPresentationContentPackFragment（项目表现内容包目录片段）。
 * 只使用逻辑DefinitionId；不携带具体资源路径。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsPresentationContentPackFragment
{
    GENERATED_BODY()

    /** 唯一内容所有者身份，不是文件夹路径；同服务中重复/版本漂移选择被拒绝。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ContentPackId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Revision = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGamePlatformPresentationContextScope LifecycleScope =
        EGamePlatformPresentationContextScope::World;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FGamePlatformPresentationCatalogFragment CatalogFragment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FName> LogicalPreloadDefinitionIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiredPreload = false;

    bool IsValid(FString& OutError) const;
};

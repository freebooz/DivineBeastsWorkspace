#pragma once

// 平台层双端私有源索引：只保存逻辑身份与软路径，不拥有资产、世界、PIE实例或加载租约。
// 所有入口仅GameThread；注册表适配与独立生产行为测试共用同一索引实现，不形成公开API。
#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "Types/GamePlatformId.h"
#include "Types/GamePlatformResult.h"
#include "UObject/PrimaryAssetId.h"
#include "UObject/SoftObjectPath.h"

class IAssetRegistry;
class UGamePlatformPrimaryDataAsset;

namespace GamePlatform::Data
{
/** 私有源访问合同。生产绑定引擎Registry；测试可控制发现与快照，不替换生产索引算法。 */
struct FDefinitionSourceIndexAccess
{
    /** 首次或后续发现尚未结束时为true；禁止在此同步等待或加载资产。 */
    TFunction<bool()> IsDiscovering;
    /** 一次取得平台主资产及派生类的源元数据；只在冷建/明确失效后调用。 */
    TFunction<void(TArray<FAssetData>&)> EnumerateDefinitions;
    /** 按真实类继承判断资格；不能只凭LogicalId标签把其他资产纳入索引。 */
    TFunction<bool(const FAssetData&)> IsDefinitionAsset;
};

/** 模块私有计数，仅描述实际查询与工作次数，不是耗时、CPU预算或性能百分比。 */
struct FDefinitionSourceIndexDiagnostics
{
    uint64 QueryCount = 0;          // 所有Resolve调用，包括参数拒绝和未就绪。
    uint64 FullScanCount = 0;       // 实际调用源枚举的次数，包括发布前被取消的扫描。
    uint64 RebuildCount = 0;        // 完整快照成功发布次数。
    uint64 AssetEventCount = 0;     // 活动期接收的增删改名/标签更新次数。
    uint64 InvalidationCount = 0;  // 发现或类拓扑变化导致的明确失效次数。
    int32 SourceCount = 0;         // 当前索引中的不同物理路径数。
    bool bActive = false;          // Shutdown后为false，不能继续解析。
    bool bBuilt = false;           // 当前是否持有已发布完整快照。
};

/**
 * LogicalId→唯一路径集合及路径→旧LogicalId反向表，处理重复修复与身份变化。
 * 冷建只在发现完成后的查询发生；普通资产事件增量更新，发现/蓝图类变化延迟重建。
 * Shutdown先解除委托并取消未发布快照，再清空值和访问回调；不会加载或释放业务资产。
 */
class FDefinitionSourceIndex final
{
public:
    FDefinitionSourceIndex() = default;
    ~FDefinitionSourceIndex();
    FDefinitionSourceIndex(const FDefinitionSourceIndex&) = delete;
    FDefinitionSourceIndex& operator=(const FDefinitionSourceIndex&) = delete;

    /** 生产入口：绑定真实注册表事件，只登记机制，不在模块启动时扫描资产。 */
    void Startup(IAssetRegistry& InRegistry);
    /** 使用相同生产算法的私有源入口；调用者负责源生命周期及有序转交资产事件。 */
    void Startup(FDefinitionSourceIndexAccess InAccess);
    /** 幂等关闭；取消在途快照并拒绝后续解析，显式解除全部引擎事件委托。 */
    void Shutdown();
    /** 发现开始/完成或类拓扑变化时清除旧快照；下一次就绪查询才允许重新枚举。 */
    void Invalidate();

    /** 同路径替换旧身份；无效ID或非定义资产会移除该路径的旧记录。 */
    void OnAssetAddedOrUpdated(const FAssetData& Asset);
    /** 删除只按物理路径撤销旧身份，不能信任删除事件携带的最后一个LogicalId。 */
    void OnAssetRemoved(const FAssetData& Asset);
    /** 原路径先撤销，再根据新路径/身份登记；不会留下重命名前的源。 */
    void OnAssetRenamed(const FAssetData& Asset, const FString& OldObjectPath);

    /** 与公开唯一源合同一致；EditorCandidate只覆盖本次查询，不写回全局索引。 */
    FGamePlatformResult Resolve(const FPrimaryAssetId& DefinitionId, FSoftObjectPath& OutSource,
        const UGamePlatformPrimaryDataAsset* EditorCandidate = nullptr);
    /** 返回值快照，仅GameThread；不触发发现、枚举或对象加载。 */
    FDefinitionSourceIndexDiagnostics GetDiagnostics() const;

private:
    /** 判断是否可发布/复用完整源快照；发现未完成、重入或关闭都拒绝发布。 */
    bool EnsureBuilt();
    /** 事件阶段仅在完整快照且发现结束时增量修改，否则立即失效。 */
    bool CanApplyAssetEvent(const FAssetData& Asset);
    /** 删除路径的旧身份与空集合，保证LogicalId变化不会保留旧可见源。 */
    void RemovePath(const FSoftObjectPath& Path);
    /** 加入已确认类资格的合法源；空路径或非法标签没有索引资格。 */
    void AddQualifiedSource(const FAssetData& Asset);

    FDefinitionSourceIndexAccess Access;
    TMap<FGamePlatformId, TSet<FSoftObjectPath>> PathsByLogicalId;
    TMap<FSoftObjectPath, FGamePlatformId> LogicalIdsByPath;
    FDefinitionSourceIndexDiagnostics Diagnostics;
    uint64 MutationRevision = 0;   // 关闭/失效/资产事件均推进，取消旧扫描发布。
    bool bActive = false;
    bool bBuilt = false;
    bool bRebuilding = false;

    // 仅引擎进程级注册表的非拥有引用；不保存资产对象或世界，Shutdown不重新加载模块。
    IAssetRegistry* BoundRegistry = nullptr;
    FDelegateHandle AssetAddedHandle;
    FDelegateHandle AssetRemovedHandle;
    FDelegateHandle AssetUpdatedHandle;
    FDelegateHandle AssetUpdatedOnDiskHandle;
    FDelegateHandle AssetRenamedHandle;
    FDelegateHandle FilesLoadedHandle;
    FDelegateHandle ScanStartedHandle;
    FDelegateHandle KnownGathersCompleteHandle;
};

/** 本模块唯一实例；由GamePlatformData模块Startup/Shutdown显式管理其活动期。 */
FDefinitionSourceIndex& GetDefinitionSourceIndex();
}

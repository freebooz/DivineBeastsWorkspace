#pragma once
#include "Engine/DataAsset.h"
#include "Types/GamePlatformId.h"
#include "GamePlatformPrimaryDataAsset.generated.h"

/** EGamePlatformDefinitionIdBuildStatus（平台逻辑定义编号解析结果）。
 * 让VFX/SFX/世界配置的调用方共用一套规范转换，并保留领域自身错误提示。
 * 没有新增资产类型、主资产路径或网络权限。 */
enum class EGamePlatformDefinitionIdBuildStatus : uint8
{
    Success,            // 规范化后成功构造平台定义主资产ID
    InvalidLogicalId,   // 缺失或不是 namespace.name@version 逻辑身份
    EmptyCanonicalId    // 理论防御分支，防止空规范值进入主资产查找
};

/** 平台主资产基类；身份与文件名称、路径、重命名无关。只在游戏线程访问UObject。 */
UCLASS(Abstract, BlueprintType)
class GAMEPLATFORMDATA_API UGamePlatformPrimaryDataAsset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** 唯一逻辑身份，保存后进入源资产注册表；已发布身份变更须迁移引用。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Data")
    FGamePlatformId LogicalId;

    /** 固定GamePlatformDefinition类型与规范逻辑字符串；非法逻辑身份返回无效ID。 */
    virtual FPrimaryAssetId GetPrimaryAssetId() const override;
    /** 源资产标签供独立重复扫描使用，不能用已去重的AssetManager字典替代。 */
    virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
    /** 主资产类型的稳定协议名，不依赖派生C++类名称。 */
    static FPrimaryAssetType DefinitionAssetType();
    /**
     * 跨游戏通用DefinitionId（定义逻辑编号）到FPrimaryAssetId（主资产身份）的唯一转换入口。
     * 输入为完整逻辑FName，不是uasset路径；失败时清空OutAssetId，避免复用旧世界/旧请求身份。
     * 不读取资源/不加载UObject；具体声音、特效、天气的错误消息由领域调用方决定。
     */
    static EGamePlatformDefinitionIdBuildStatus BuildDefinitionAssetId(
        FName DefinitionId, FPrimaryAssetId& OutAssetId);
    /** 源注册表中的稳定逻辑身份标签。 */
    static FName LogicalIdTag();
};

#pragma once
#include "Engine/DataAsset.h"
#include "Types/GamePlatformId.h"
#include "GamePlatformPrimaryDataAsset.generated.h"

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
    /** 源注册表中的稳定逻辑身份标签。 */
    static FName LogicalIdTag();
};

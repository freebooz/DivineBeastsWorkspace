#pragma once
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Types/GamePlatformDataVersion.h"
#include "Types/GamePlatformResult.h"
#include "GamePlatformDefinitionBase.generated.h"

/** 运行期只读定义；服务成功前递归持有全部必需定义，失败回滚整条租约。 */
UCLASS(BlueprintType)
class GAMEPLATFORMDATA_API UGamePlatformDefinitionBase : public UGamePlatformPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** 资产声明的结构及内容版本；运行期不得原地改动共享定义。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Data")
    FGamePlatformDataVersion DataVersion;
    /** 必需定义主资产身份；与根定义共享请求分组和租约期限，缺失或环使整个请求失败。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Data")
    TArray<FPrimaryAssetId> RequiredDefinitions;
    /** 最低可读结构版本；由派生类代码覆盖，服务从类CDO读取，禁止依赖资产编辑值。 */
    virtual int32 GetMinimumReadableSchemaVersion() const;
    /** 最高可读结构版本；默认只读结构1，无自动迁移。 */
    virtual int32 GetMaximumReadableSchemaVersion() const;
    /** 校验身份、CDO范围、正修订和依赖ID；派生类可扩展必填检查，须先调用Super。不执行加载。 */
    virtual FGamePlatformResult ValidateDefinition() const;
};

#pragma once

#include "Definitions/GamePlatformDefinitionBase.h"
#include "UObject/SoftObjectPtr.h"
#include "GamePlatformWorldDefinition.generated.h"

class UWorld;

/** 中立世界内容定义；WorldId就是继承的LogicalId，地图重命名不改变逻辑身份。
 * 游戏线程只读；不含端点、准入材料或服务器分配。DataVersion与RequiredDefinitions沿用Data体系。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMWORLD_API UGamePlatformWorldDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    /** 已保存地图的顶层UWorld软身份；运行期校验不加载、不Travel，存在性与类型由编辑器验证。
     * 不自动作为资产Bundle预加载整张地图；实际地图进入与流送由各自所有者协调。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|World")
    TSoftObjectPtr<UWorld> MapIdentity;

    /** 可选中立体验身份；默认空值表示未指定，半填或非法版本拒绝，不是服务器角色。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|World")
    FGamePlatformId DefaultExperienceId;

    /** 本世界必需区域的Data主资产身份；无重复且每项必须同时位于RequiredDefinitions。
     * 这样真实Data租约递归持有所需区域，不能仅凭区域列表假定依赖已加载。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|World")
    TArray<FPrimaryAssetId> Regions;

    /** 世界就绪等待上限，单位秒，默认60；必须有限且大于零，不以到期代替真实就绪。
     * 这里只声明策略，运行世界服务负责基于单调时间执行截止与失败收敛。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|World", meta=(ClampMin="0.001", Units="s"))
    float ReadinessTimeoutSeconds = 60.f;

    /** 游戏线程纯字段校验，先执行Data基类验证；失败给稳定错误码，不加载资产或访问网络。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};

// 平台中立天气预设DataAsset定义；游戏项目只需创建Definition实例，不派生一套相同的天气类。
#pragma once
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "GamePlatformWeatherPresetDefinition.generated.h"

/** Data负责资源ID、结构版本、递归租约；天气层仅验证天气取值与调度规则。 */
UCLASS(BlueprintType)
class GAMEPLATFORMWEATHERRUNTIME_API UGamePlatformWeatherPresetDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="GamePlatform|Weather")
    FGamePlatformWeatherScheduleEntry Weather;
    /** 无副作用校验，不加载任何粒子、声音或材质资产。 */
    virtual FGamePlatformResult ValidateDefinition() const override;
};

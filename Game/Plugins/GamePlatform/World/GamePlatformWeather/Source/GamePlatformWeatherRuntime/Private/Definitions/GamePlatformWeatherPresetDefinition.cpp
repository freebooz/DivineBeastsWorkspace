// 天气数据资产验证；平台层不绑定神兽联盟世界纹理或项目语义。
#include "Definitions/GamePlatformWeatherPresetDefinition.h"
FGamePlatformResult UGamePlatformWeatherPresetDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess()) return BaseResult;
    return Weather.IsValid() ? FGamePlatformResult::Success() :
        FGamePlatformResult::Failure(TEXT("InvalidWeatherPreset"), TEXT("天气预设包含非法强度、温度、持续时间、过渡秒数或权重。"));
}

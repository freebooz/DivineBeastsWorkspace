#include "Definitions/GamePlatformWorldUIDefinition.h"

FGamePlatformResult UGamePlatformWorldUIDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess())
    {
        return BaseResult;
    }

    if (WidgetClass.IsNull())
    {
        return FGamePlatformResult::Failure(
            TEXT("UI.WorldUI.MissingWidgetClass"),
            TEXT("世界UI Definition必须配置WidgetClass软类。"));
    }

    if (!FMath::IsFinite(DefaultMaxVisibleDistance) ||
        DefaultMaxVisibleDistance < 0.0f)
    {
        return FGamePlatformResult::Failure(
            TEXT("UI.WorldUI.InvalidDistance"),
            TEXT("世界UI默认最大可见距离必须为有限非负值。"));
    }

    return FGamePlatformResult::Success();
}

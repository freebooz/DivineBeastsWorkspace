#include "Definitions/GamePlatformFeedbackDefinition.h"

FGamePlatformResult UGamePlatformFeedbackDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess())
    {
        return BaseResult;
    }

    if (Channel.IsNone())
    {
        return FGamePlatformResult::Failure(
            TEXT("UI.Feedback.MissingChannel"),
            TEXT("反馈Definition必须配置Channel（反馈通道）。"));
    }

    if (WidgetClass.IsNull())
    {
        return FGamePlatformResult::Failure(
            TEXT("UI.Feedback.MissingWidgetClass"),
            TEXT("反馈Definition必须配置WidgetClass软类。"));
    }

    if (!FMath::IsFinite(DefaultLifetimeSeconds) ||
        DefaultLifetimeSeconds < 0.05f)
    {
        return FGamePlatformResult::Failure(
            TEXT("UI.Feedback.InvalidLifetime"),
            TEXT("反馈默认生命周期必须为有限且不小于0.05秒。"));
    }

    return FGamePlatformResult::Success();
}

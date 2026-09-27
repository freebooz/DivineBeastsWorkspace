#include "Definitions/GamePlatformNotificationDefinition.h"

FGamePlatformResult UGamePlatformNotificationDefinition::ValidateDefinition() const
{
    const FGamePlatformResult BaseResult = Super::ValidateDefinition();
    if (!BaseResult.IsSuccess())
    {
        return BaseResult;
    }

    if (Channel.IsNone())
    {
        return FGamePlatformResult::Failure(
            TEXT("UI.Notification.MissingChannel"),
            TEXT("通知Definition必须配置Channel（通知通道）。"));
    }

    if (WidgetClass.IsNull())
    {
        return FGamePlatformResult::Failure(
            TEXT("UI.Notification.MissingWidgetClass"),
            TEXT("通知Definition必须配置WidgetClass软类。"));
    }

    if (!FMath::IsFinite(DefaultLifetimeSeconds) ||
        DefaultLifetimeSeconds < 0.0f)
    {
        return FGamePlatformResult::Failure(
            TEXT("UI.Notification.InvalidLifetime"),
            TEXT("通知默认生命周期必须为有限非负秒数。"));
    }

    return FGamePlatformResult::Success();
}

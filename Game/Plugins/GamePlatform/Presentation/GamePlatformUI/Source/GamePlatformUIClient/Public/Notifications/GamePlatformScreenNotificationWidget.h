#pragma once

#include "Notifications/GamePlatformNotificationWidget.h"
#include "GamePlatformScreenNotificationWidget.generated.h"

/**
 * UGamePlatformScreenNotificationWidget（游戏平台屏幕通知基类）。
 *
 * 用于 CenterMessage（中央消息）、Reward（奖励）、Achievement（成就）和 System（系统公告）等
 * 比 Toast 更显著的通知表面。具体通道差异优先由 Definition / Style 驱动，不机械创建业务 C++ 子类。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformScreenNotificationWidget
    : public UGamePlatformNotificationWidget
{
    GENERATED_BODY()
};

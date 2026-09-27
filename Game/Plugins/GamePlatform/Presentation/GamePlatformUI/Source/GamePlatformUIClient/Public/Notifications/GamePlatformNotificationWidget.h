#pragma once

#include "Core/GamePlatformWidgetBase.h"
#include "GamePlatformNotificationWidget.generated.h"

/**
 * UGamePlatformNotificationWidget（游戏平台通知基类）。
 *
 * 用于 Toast（短提示）、奖励、任务、成就、解锁和系统公告等非页面型信息。
 * 通知由 Notification Layer（通知层）管理，不进入 CommonUI 页面导航栈。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformNotificationWidget
    : public UGamePlatformWidgetBase
{
    GENERATED_BODY()
};

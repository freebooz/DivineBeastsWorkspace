#pragma once

#include "Core/GamePlatformWidgetBase.h"
#include "Requests/GamePlatformUINotificationRequest.h"
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

public:
    /** 由 NotificationService 统一应用请求；业务Widget不自行拥有通知队列。 */
    void ApplyNotificationRequest(
        const FGamePlatformUINotificationRequest& InRequest);

    const FGamePlatformUINotificationRequest& GetNotificationRequest() const
    {
        return NotificationRequest;
    }

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Notification", meta=(DisplayName="通知请求已应用"))
    void BP_OnNotificationRequestApplied(
        FGamePlatformUINotificationRequest Request);

private:
    UPROPERTY(Transient)
    FGamePlatformUINotificationRequest NotificationRequest;
};

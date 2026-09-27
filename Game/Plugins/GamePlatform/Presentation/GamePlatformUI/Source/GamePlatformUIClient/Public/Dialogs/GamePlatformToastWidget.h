#pragma once

#include "Notifications/GamePlatformNotificationWidget.h"
#include "GamePlatformToastWidget.generated.h"

/**
 * UGamePlatformToastWidget（游戏平台短提示基类）。
 *
 * Toast 属于 Notification（通知）分类，不抢输入焦点、不进入页面栈。
 * 显示时长、优先级和去重键由 Manager 统一处理，业务模块只提交展示数据。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformToastWidget
    : public UGamePlatformNotificationWidget
{
    GENERATED_BODY()

public:
    UPROPERTY(BlueprintReadOnly, Category="UI|Toast")
    FName ToastKey = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="UI|Toast")
    int32 ToastPriority = 0;

    UPROPERTY(BlueprintReadOnly, Category="UI|Toast")
    float DurationSeconds = 2.0f;

    UFUNCTION(BlueprintCallable, Category="UI|Toast")
    void ConfigureToast(FName InToastKey, int32 InPriority, float InDurationSeconds)
    {
        ToastKey = InToastKey;
        ToastPriority = InPriority;
        DurationSeconds = FMath::Max(0.0f, InDurationSeconds);
    }
};

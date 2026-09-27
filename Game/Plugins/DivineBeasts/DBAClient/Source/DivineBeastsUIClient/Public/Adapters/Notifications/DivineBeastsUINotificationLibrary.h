#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Requests/GamePlatformUINotificationRequest.h"
#include "DivineBeastsUINotificationLibrary.generated.h"

/**
 * UDivineBeastsUINotificationLibrary（神兽联盟通知请求工厂）。
 *
 * 只负责把项目语义映射为平台中立Channel/StyleId；
 * 队列、优先级、去重和生命周期由GamePlatformNotificationService统一处理。
 */
UCLASS()
class DIVINEBEASTSUICLIENT_API UDivineBeastsUINotificationLibrary
    : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Notification")
    static FGamePlatformUINotificationRequest MakeToast(
        FName NotificationKey,
        FText Message,
        int32 Priority = 0,
        float LifetimeSeconds = 2.0f);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Notification")
    static FGamePlatformUINotificationRequest MakeCenterMessage(
        FName NotificationKey,
        FText Message,
        int32 Priority = 50,
        float LifetimeSeconds = 2.5f);

    UFUNCTION(BlueprintPure, Category="DivineBeasts|UI|Notification")
    static FGamePlatformUINotificationRequest MakeRewardMessage(
        FName NotificationKey,
        FText Title,
        FText Message,
        int32 Priority = 25,
        float LifetimeSeconds = 3.0f);
};

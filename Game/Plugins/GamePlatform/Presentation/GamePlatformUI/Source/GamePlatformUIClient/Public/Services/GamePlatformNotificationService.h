#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "Requests/GamePlatformUINotificationRequest.h"
#include "UObject/Object.h"
#include "GamePlatformNotificationService.generated.h"

class UGamePlatformNotificationWidget;
class UGamePlatformToastWidget;
class UGamePlatformUILayerStack;
class ULocalPlayer;

/**
 * UGamePlatformNotificationService（游戏平台通知服务）。
 *
 * 由 UGamePlatformUIManagerSubsystem（UI管理子系统）持有，不单独创建 Subsystem。
 * 负责通知的同键去重、优先级替换、自动关闭和 NotificationLayer（通知层）装配。
 * 业务层只提交中立 Request，不直接 AddToViewport。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformNotificationService : public UObject
{
    GENERATED_BODY()

public:
    /** 绑定所属 LocalPlayer；服务不拥有玩家生命周期。 */
    void Initialize(ULocalPlayer* InLocalPlayer);

    /** 根布局创建/替换后更新目标层；传入 nullptr 表示暂时不可显示。 */
    void SetRootLayout(UGamePlatformUILayerStack* InRootLayout);

    /** 创建并提交一条通知；返回无效Guid表示被限流、参数非法或创建失败。 */
    UFUNCTION(BlueprintCallable, Category="UI|Notification")
    FGuid SubmitNotification(
        FGamePlatformUINotificationRequest Request,
        TSubclassOf<UGamePlatformNotificationWidget> WidgetClass);

    /** 兼容旧 Toast 调用入口；实际生命周期统一转交本服务。 */
    bool AttachToastWidget(UGamePlatformToastWidget* Widget);

    /** 显式关闭指定通知。 */
    UFUNCTION(BlueprintCallable, Category="UI|Notification")
    bool DismissNotification(FGuid RequestId);

    /** 清理所有活动通知和计时器；Travel/Deinitialize时调用。 */
    void Clear();

private:
    struct FActiveNotification
    {
        TWeakObjectPtr<UGamePlatformNotificationWidget> Widget;
        FName NotificationKey = NAME_None;
        int32 Priority = 0;
        FTimerHandle TimerHandle;
    };

    bool AttachNotificationWidget(
        UGamePlatformNotificationWidget* Widget,
        const FGamePlatformUINotificationRequest& Request);

    UWorld* GetServiceWorld() const;

    UPROPERTY(Transient)
    TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUILayerStack> RootLayout = nullptr;

    TMap<FGuid, FActiveNotification> ActiveNotifications;
    TMap<FName, FGuid> ActiveByKey;
};

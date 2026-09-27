#pragma once

#include "Definitions/GamePlatformUIDefinitionBase.h"
#include "Notifications/GamePlatformNotificationWidget.h"
#include "GamePlatformNotificationDefinition.generated.h"

/** UGamePlatformNotificationDefinition（游戏平台通知定义）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMUICLIENT_API UGamePlatformNotificationDefinition
    : public UGamePlatformUIDefinitionBase
{
    GENERATED_BODY()

public:
    /** 通知通道，例如Toast、CenterMessage、Reward、System。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Notification")
    FName Channel = NAME_None;

    /** 通知视觉Widget软类；必须继承平台Notification基类。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Notification")
    TSoftClassPtr<UGamePlatformNotificationWidget> WidgetClass;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Notification", meta=(ClampMin="0.0"))
    float DefaultLifetimeSeconds = 2.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Notification")
    int32 DefaultPriority = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Notification")
    bool bReplaceSameKey = true;

    virtual FGamePlatformResult ValidateDefinition() const override;
};

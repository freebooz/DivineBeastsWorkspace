#pragma once

#include "CoreMinimal.h"
#include "GamePlatformUINotificationRequest.generated.h"

/**
 * FGamePlatformUINotificationRequest（游戏平台屏幕通知请求）。
 *
 * 该结构只描述“显示什么、以什么通道显示以及生命周期约束”，不包含任何具体游戏业务语义。
 * 奖励、任务、成就、系统公告等上层领域应通过 Channel（通知通道）和 StyleId（样式身份）
 * 映射到 Definition（定义资产），而不是在平台层新增业务专用 Widget 类。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUINotificationRequest
{
    GENERATED_BODY()

    /** 本次通知请求的唯一身份；未设置时服务会自动生成。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Notification")
    FGuid RequestId;

    /** 稳定通知键；用于同类通知去重、替换和优先级比较。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Notification")
    FName NotificationKey = NAME_None;

    /** 通知通道，例如 Toast、CenterMessage、Reward、System；平台不解释业务含义。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Notification")
    FName Channel = NAME_None;

    /** 数据驱动样式身份；由项目层或内容包解析为具体视觉资源。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Notification")
    FName StyleId = NAME_None;

    /** 可选标题。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Notification")
    FText Title;

    /** 主体文本。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Notification")
    FText Message;

    /** 优先级；同键替换时数值更高的请求优先。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Notification")
    int32 Priority = 0;

    /** 自动关闭时长，单位秒；小于等于0表示由调用方显式关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Notification", meta=(ClampMin="0.0"))
    float LifetimeSeconds = 2.0f;

    /** 同一 NotificationKey 已存在时是否允许替换。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Notification")
    bool bReplaceSameKey = true;

    /** 基础结构校验；业务资格由上层领域自行判断。 */
    bool IsValid() const
    {
        return !Channel.IsNone() || !NotificationKey.IsNone();
    }
};

#pragma once

#include "CoreMinimal.h"
#include "GamePlatformUIFeedbackRequest.generated.h"

/**
 * FGamePlatformUIFeedbackRequest（游戏平台高频反馈请求）。
 *
 * 用于伤害飘字、治疗反馈、命中反馈、拾取反馈等高频但非权威的视觉信息。
 * 平台层不认识 Damage（伤害）、Critical（暴击）等业务枚举；上层通过 Channel、StyleId
 * 和展示文本表达语义，从而保证该机制可以直接复用于其他游戏项目。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMUICLIENT_API FGamePlatformUIFeedbackRequest
{
    GENERATED_BODY()

    /** 唯一发生身份，用于网络确认、预测去重和反馈实例追踪。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    FGuid OccurrenceId;

    /** 反馈通道，例如 FloatingText、Hit、Pickup；平台仅用于分组。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    FName Channel = NAME_None;

    /** 样式身份，由 Definition / Theme 解析具体表现。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    FName StyleId = NAME_None;

    /** 可选合并键；同键短时间重复请求可以合并，减少高频 Widget 数量。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    FName MergeKey = NAME_None;

    /** 展示文本；数值型反馈也可以由项目层提前格式化。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    FText Text;

    /** 可选数值；仅在 bHasNumericValue=true 时参与通用数值合并。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    double NumericValue = 0.0;

    /** 是否携带可合并数值。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    bool bHasNumericValue = false;

    /** 是否允许平台服务按 MergeKey 合并当前反馈。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    bool bAllowMerge = false;

    /** 是否使用世界坐标作为初始投影位置。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    bool bUseWorldLocation = false;

    /** 反馈发生的世界坐标；平台服务只负责初始投影，不拥有权威实体。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    FVector WorldLocation = FVector::ZeroVector;

    /** 已解析或调用方提供的屏幕坐标。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    FVector2D ScreenPosition = FVector2D::ZeroVector;

    /** 显示优先级；达到并发上限时低优先级请求可被拒绝。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback")
    int32 Priority = 0;

    /** 生命周期秒数；反馈层默认按短生命周期对象池复用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UI|Feedback", meta=(ClampMin="0.05"))
    float LifetimeSeconds = 1.0f;

    bool IsValid() const
    {
        return !Channel.IsNone() && LifetimeSeconds > 0.0f;
    }
};

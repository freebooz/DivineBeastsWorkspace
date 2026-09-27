#pragma once

#include "Core/GamePlatformWidgetBase.h"
#include "Requests/GamePlatformUIFeedbackRequest.h"
#include "GamePlatformFeedbackWidget.generated.h"

/**
 * UGamePlatformFeedbackWidget（游戏平台高频反馈界面基类）。
 *
 * 由 FeedbackService（反馈服务）统一创建、复用和回收；业务代码不得直接 AddToViewport。
 * Widget 只持有本次视觉请求，不拥有伤害、治疗、拾取等权威业务状态。
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMUICLIENT_API UGamePlatformFeedbackWidget
    : public UGamePlatformWidgetBase
{
    GENERATED_BODY()

public:
    /** 应用一条新的反馈请求，并通知 Blueprint 刷新视觉。 */
    void ApplyFeedbackRequest(const FGamePlatformUIFeedbackRequest& InRequest);

    /**
     * 尝试与当前请求合并。
     * 平台只实现“同 MergeKey + 双方均允许 + 数值型”的安全合并，不猜测业务语义。
     */
    bool MergeFeedbackRequest(const FGamePlatformUIFeedbackRequest& InRequest);

    /** 回收到对象池前清理瞬态请求。 */
    void ResetFeedbackState();

    UFUNCTION(BlueprintPure, Category="UI|Feedback")
    const FGamePlatformUIFeedbackRequest& GetFeedbackRequest() const
    {
        return FeedbackRequest;
    }

protected:
    /** Blueprint 根据请求更新文本、动画和样式。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Feedback", meta=(DisplayName="反馈请求已应用"))
    void BP_OnFeedbackRequestApplied(FGamePlatformUIFeedbackRequest Request);

    /** Blueprint 在数值合并后执行局部动画强化，不重新创建 Widget。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Feedback", meta=(DisplayName="反馈请求已合并"))
    void BP_OnFeedbackRequestMerged(FGamePlatformUIFeedbackRequest Request);

    /** 对象返回池前清理动画和瞬态视觉状态。 */
    UFUNCTION(BlueprintImplementableEvent, Category="UI|Feedback", meta=(DisplayName="反馈控件即将回收"))
    void BP_OnFeedbackRecycled();

private:
    UPROPERTY(Transient)
    FGamePlatformUIFeedbackRequest FeedbackRequest;
};

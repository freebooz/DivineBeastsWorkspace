#include "Feedback/GamePlatformFeedbackWidget.h"

void UGamePlatformFeedbackWidget::ApplyFeedbackRequest(
    const FGamePlatformUIFeedbackRequest& InRequest)
{
    FeedbackRequest = InRequest;
    SetVisibility(ESlateVisibility::HitTestInvisible);
    BP_OnFeedbackRequestApplied(FeedbackRequest);
}

bool UGamePlatformFeedbackWidget::MergeFeedbackRequest(
    const FGamePlatformUIFeedbackRequest& InRequest)
{
    if (!InRequest.IsValid() || !FeedbackRequest.bAllowMerge ||
        !InRequest.bAllowMerge ||
        FeedbackRequest.MergeKey.IsNone() ||
        FeedbackRequest.MergeKey != InRequest.MergeKey ||
        FeedbackRequest.Channel != InRequest.Channel ||
        FeedbackRequest.StyleId != InRequest.StyleId ||
        !FeedbackRequest.bHasNumericValue ||
        !InRequest.bHasNumericValue)
    {
        return false;
    }

    // 两个有限double仍可能相加溢出；失败保持旧文本/计时不变。
    const double CombinedValue = FeedbackRequest.NumericValue + InRequest.NumericValue;
    if (!FMath::IsFinite(CombinedValue)) return false;

    // 平台只执行数值相加；文本格式、正负号、暴击等业务表达由上层 Definition / Blueprint 决定。
    FeedbackRequest.NumericValue = CombinedValue;
    FeedbackRequest.Text = InRequest.Text;
    FeedbackRequest.Priority = FMath::Max(
        FeedbackRequest.Priority,
        InRequest.Priority);
    FeedbackRequest.LifetimeSeconds = FMath::Max(
        FeedbackRequest.LifetimeSeconds,
        InRequest.LifetimeSeconds);
    BP_OnFeedbackRequestMerged(FeedbackRequest);
    return true;
}

void UGamePlatformFeedbackWidget::ResetFeedbackState()
{
    BP_OnFeedbackRecycled();
    FeedbackRequest = FGamePlatformUIFeedbackRequest();
    SetVisibility(ESlateVisibility::Collapsed);
}

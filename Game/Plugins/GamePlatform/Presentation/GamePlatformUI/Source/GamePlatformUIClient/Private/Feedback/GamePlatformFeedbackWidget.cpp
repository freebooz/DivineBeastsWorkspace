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
    if (!FeedbackRequest.bAllowMerge ||
        !InRequest.bAllowMerge ||
        FeedbackRequest.MergeKey.IsNone() ||
        FeedbackRequest.MergeKey != InRequest.MergeKey ||
        !FeedbackRequest.bHasNumericValue ||
        !InRequest.bHasNumericValue)
    {
        return false;
    }

    // 平台只执行数值相加；文本格式、正负号、暴击等业务表达由上层 Definition / Blueprint 决定。
    FeedbackRequest.NumericValue += InRequest.NumericValue;
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

// 平台层客户端浮字：复用反馈服务对象池，只渲染中立请求；字体/材质由上层Monolith蓝图拥有。
#include "Feedback/GamePlatformFloatingTextWidget.h"
#include "Components/TextBlock.h"

void UGamePlatformFloatingTextWidget::RefreshFloatingText()
{
    const auto& Request = GetFeedbackRequest();
    if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("FeedbackText"))))
    {
        // 累计值直接格式化double，避免大数转int32溢出；无数值时保留业务本地化文本。
        FNumberFormattingOptions Format;
        Format.SetMaximumFractionalDigits(0);
        Label->SetText(Request.bHasNumericValue ? FText::AsNumber(Request.NumericValue, &Format) : Request.Text);
        Label->SetColorAndOpacity(FSlateColor(Request.TextColor));
    }
    SetRenderTranslation(GetFeedbackRequest().ScreenPosition);
    SetRenderOpacity(1.0f);
}

void UGamePlatformFloatingTextWidget::ApplyFeedbackRequest(const FGamePlatformUIFeedbackRequest& InRequest)
{
    VisualElapsedSeconds = 0.0f;
    Super::ApplyFeedbackRequest(InRequest);
    RefreshFloatingText();
}

bool UGamePlatformFloatingTextWidget::MergeFeedbackRequest(const FGamePlatformUIFeedbackRequest& InRequest)
{
    if (!Super::MergeFeedbackRequest(InRequest)) return false;
    VisualElapsedSeconds = 0.0f;
    RefreshFloatingText();
    return true;
}

void UGamePlatformFloatingTextWidget::ResetFeedbackState()
{
    VisualElapsedSeconds = 0.0f;
    SetRenderTranslation(FVector2D::ZeroVector);
    SetRenderOpacity(1.0f);
    if (UTextBlock* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("FeedbackText")))) Label->SetText(FText::GetEmpty());
    Super::ResetFeedbackState();
}

void UGamePlatformFloatingTextWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    if (!FMath::IsFinite(InDeltaTime) || InDeltaTime < 0.0f) return;
    VisualElapsedSeconds += InDeltaTime;
    const auto& Request = GetFeedbackRequest();
    const float Alpha = FMath::Clamp(VisualElapsedSeconds / FMath::Max(0.05f, Request.LifetimeSeconds), 0.0f, 1.0f);
    // 56 Slate单位的短距上浮；前60%保持清晰，末段淡出。不使用世界坐标持续追随已死亡目标。
    SetRenderTranslation(Request.ScreenPosition + FVector2D(0.0f, -56.0f * Alpha));
    SetRenderOpacity(1.0f - FMath::Clamp((Alpha - 0.6f) / 0.4f, 0.0f, 1.0f));
}

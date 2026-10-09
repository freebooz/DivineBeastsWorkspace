#include "Components/GamePlatformCastProgressWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"

bool FGamePlatformUICastProgressPresentation::IsValidSnapshot(
    const FGamePlatformUICastProgressState& InState)
{
    // 视觉安全上限是7天；不允许NaN/Inf或由异常网络包引起数值溢出。
    constexpr double MaxVisibleCastSeconds = 7.0 * 24.0 * 60.0 * 60.0;
    return InState.SourceScopeId.IsValid() &&
        !InState.CastInstanceId.IsNone() &&
        InState.Revision >= 0 &&
        FMath::IsFinite(InState.TotalSeconds) &&
        FMath::IsFinite(InState.RemainingSeconds) &&
        InState.TotalSeconds >= 0.0 &&
        InState.RemainingSeconds >= 0.0 &&
        InState.TotalSeconds <= MaxVisibleCastSeconds &&
        InState.RemainingSeconds <= MaxVisibleCastSeconds &&
        (InState.TotalSeconds <= 0.0 ||
         InState.RemainingSeconds <= InState.TotalSeconds);
}

float FGamePlatformUICastProgressPresentation::GetNormalizedProgress(
    const FGamePlatformUICastProgressState& InState)
{
    if (!IsValidSnapshot(InState) || !InState.bActive ||
        InState.TotalSeconds <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }
    const double Elapsed = 1.0 -
        InState.RemainingSeconds / InState.TotalSeconds;
    // 表现进度与权威施法结束不同步时，只显示经验证的快照比例。
    return static_cast<float>(FMath::Clamp(Elapsed, 0.0, 1.0));
}

bool UGamePlatformCastProgressWidget::BindCastSource(FGuid InScopeId)
{
    if (!InScopeId.IsValid())
    {
        return false;
    }
    if (BoundScopeId == InScopeId)
    {
        return true;
    }
    BoundScopeId = InScopeId;
    State = FGamePlatformUICastProgressState();
    // 尚未收到可信施法快照时不展示空施法条。
    SetVisibility(ESlateVisibility::Collapsed);
    if (CastProgressBar)
    {
        CastProgressBar->SetPercent(0.0f);
    }
    if (CastNameText)
    {
        CastNameText->SetText(FText::GetEmpty());
    }
    BP_OnCastStateChanged(State);
    return true;
}

bool UGamePlatformCastProgressWidget::ApplyCastSnapshot(
    const FGamePlatformUICastProgressState& InState)
{
    if (!BoundScopeId.IsValid() ||
        InState.SourceScopeId != BoundScopeId ||
        !FGamePlatformUICastProgressPresentation::IsValidSnapshot(InState) ||
        InState.Revision <= State.Revision)
    {
        return false;
    }

    State = InState;
    if (CastProgressBar)
    {
        CastProgressBar->SetPercent(GetNormalizedCastProgress());
    }
    if (CastNameText)
    {
        CastNameText->SetText(State.bActive ? State.DisplayName : FText::GetEmpty());
    }
    // 客户端仅按经授权的bActive（当前施法状态）展开控件，不预测真正技能完成。
    SetVisibility(State.bActive
        ? ESlateVisibility::SelfHitTestInvisible
        : ESlateVisibility::Collapsed);
    BP_OnCastStateChanged(State);
    return true;
}

void UGamePlatformCastProgressWidget::ClearCastSource()
{
    if (!BoundScopeId.IsValid())
    {
        return;
    }
    BoundScopeId.Invalidate();
    State = FGamePlatformUICastProgressState();
    SetVisibility(ESlateVisibility::Collapsed);
    if (CastProgressBar)
    {
        CastProgressBar->SetPercent(0.0f);
    }
    if (CastNameText)
    {
        CastNameText->SetText(FText::GetEmpty());
    }
    BP_OnCastStateChanged(State);
}

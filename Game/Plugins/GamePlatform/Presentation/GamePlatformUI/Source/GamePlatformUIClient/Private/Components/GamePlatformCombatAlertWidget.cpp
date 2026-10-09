#include "Components/GamePlatformCombatAlertWidget.h"

#include "Components/TextBlock.h"

bool FGamePlatformUICombatAlertPresentation::IsValidSnapshot(
    const FGamePlatformUICombatAlertState& InState)
{
    // 7天仅为错误输入保护；UI无权因此认定玩法倒计时结束。
    constexpr float MaxVisibleSeconds = 7.0f * 24.0f * 60.0f * 60.0f;
    return InState.SourceScopeId.IsValid() &&
        !InState.AlertInstanceId.IsNone() &&
        InState.Revision >= 0 &&
        FMath::IsFinite(InState.RemainingSeconds) &&
        InState.RemainingSeconds >= -1.0f &&
        InState.RemainingSeconds <= MaxVisibleSeconds;
}

bool UGamePlatformCombatAlertWidget::BindAlertSource(FGuid InSourceScopeId)
{
    if (!InSourceScopeId.IsValid())
    {
        return false;
    }
    if (BoundScopeId == InSourceScopeId)
    {
        return true;
    }
    BoundScopeId = InSourceScopeId;
    State = FGamePlatformUICombatAlertState();
    if (AlertTitleText)
    {
        AlertTitleText->SetText(FText::GetEmpty());
    }
    BP_OnCombatAlertChanged(State);
    return true;
}

bool UGamePlatformCombatAlertWidget::ApplyAlertSnapshot(
    const FGamePlatformUICombatAlertState& InState)
{
    if (!BoundScopeId.IsValid() ||
        InState.SourceScopeId != BoundScopeId ||
        !FGamePlatformUICombatAlertPresentation::IsValidSnapshot(InState) ||
        InState.Revision <= State.Revision)
    {
        return false;
    }
    State = InState;
    if (AlertTitleText)
    {
        AlertTitleText->SetText(State.bActive ? State.AlertTitle : FText::GetEmpty());
    }
    BP_OnCombatAlertChanged(State);
    return true;
}

void UGamePlatformCombatAlertWidget::ClearAlertSource()
{
    if (!BoundScopeId.IsValid())
    {
        return;
    }
    BoundScopeId.Invalidate();
    State = FGamePlatformUICombatAlertState();
    if (AlertTitleText)
    {
        AlertTitleText->SetText(FText::GetEmpty());
    }
    BP_OnCombatAlertChanged(State);
}

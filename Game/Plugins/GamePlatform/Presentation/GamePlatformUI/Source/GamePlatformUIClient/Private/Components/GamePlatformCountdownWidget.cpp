#include "Components/GamePlatformCountdownWidget.h"

#include "Components/TextBlock.h"

bool UGamePlatformCountdownWidget::ApplyCountdownState(
    const FGamePlatformUICountdownState& InState)
{
    // 拒绝恶意或错误同步的NaN/Inf，以免格式化溢出、进度动画异常。
    constexpr double MaxDisplaySeconds = 2592000.0; // UI最多展示30天，不限制业务计时器本身。
    if (InState.CountdownId.IsNone() || InState.Revision < 0 ||
        !FMath::IsFinite(InState.RemainingSeconds) ||
        !FMath::IsFinite(InState.TotalSeconds) ||
        InState.RemainingSeconds < 0.0 ||
        InState.TotalSeconds < 0.0 ||
        InState.RemainingSeconds > MaxDisplaySeconds ||
        InState.TotalSeconds > MaxDisplaySeconds)
    {
        return false;
    }
    if (State.CountdownId == InState.CountdownId &&
        InState.Revision <= State.Revision)
    {
        return false;
    }

    State = InState;
    if (IsValid(CountdownText))
    {
        CountdownText->SetText(GetCountdownText());
    }
    // 仅已确认处于活动状态的赛事/任务倒计时可见；UI不得逐帧自行推演权威剩余秒数。
    SetVisibility(State.bActive
        ? ESlateVisibility::SelfHitTestInvisible
        : ESlateVisibility::Collapsed);
    BP_OnCountdownChanged(State);
    return true;
}

FText UGamePlatformCountdownWidget::GetCountdownText() const
{
    if (State.CountdownId.IsNone())
    {
        return FText::FromString(TEXT("--:--"));
    }

    // 向上取整避免计时器还剩0.3秒时提前显示00:00。
    const int64 Seconds = FMath::CeilToInt64(State.RemainingSeconds);
    const int64 Hours = Seconds / 3600;
    const int64 Minutes = (Seconds / 60) % 60;
    const int64 SecondsPart = Seconds % 60;
    const FString Value = Hours > 0
        ? FString::Printf(TEXT("%02lld:%02lld:%02lld"),
            static_cast<long long>(Hours),
            static_cast<long long>(Minutes),
            static_cast<long long>(SecondsPart))
        : FString::Printf(TEXT("%02lld:%02lld"),
            static_cast<long long>(Seconds / 60),
            static_cast<long long>(SecondsPart));
    return FText::FromString(Value);
}

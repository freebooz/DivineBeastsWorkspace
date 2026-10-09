#include "Components/GamePlatformTargetFrameWidget.h"

namespace
{
/** 只针对已经准许展示的资源条做数值范围校验，避免异常显示和浮点溢出。 */
bool IsResourceStateValid(const FGamePlatformUIResourceBarState& Resource)
{
    constexpr double MaxDisplayResource = 1.0e12;
    return FMath::IsFinite(Resource.CurrentValue) &&
        FMath::IsFinite(Resource.MaximumValue) &&
        Resource.CurrentValue >= 0.0 &&
        Resource.MaximumValue >= 0.0 &&
        Resource.CurrentValue <= MaxDisplayResource &&
        Resource.MaximumValue <= MaxDisplayResource;
}
}

bool FGamePlatformUITargetFramePresentation::IsValidSnapshot(
    const FGamePlatformUITargetFrameState& InState)
{
    if (!InState.SourceScopeId.IsValid() ||
        InState.TargetDisplayId.IsNone() ||
        InState.Revision < 0)
    {
        return false;
    }
    // bVisible=false表示权威视图禁止继续展示，旧资源无需甚至不应参与验证。
    return !InState.bVisible ||
        (IsResourceStateValid(InState.Health) &&
         IsResourceStateValid(InState.Shield));
}

void UGamePlatformTargetFrameWidget::ResetTargetVisuals()
{
    // 可见性撤销时不仅清空Widget自身数据，还要撤销每个已绑定子控件的旧肖像/资源。
    if (TargetPortrait)
    {
        TargetPortrait->ApplyPortraitState(FGamePlatformUIPortraitState());
    }
    if (TargetHealthBar)
    {
        TargetHealthBar->ApplyResourceState(FGamePlatformUIResourceBarState());
    }
    if (TargetShieldBar)
    {
        TargetShieldBar->ApplyResourceState(FGamePlatformUIResourceBarState());
    }
}

bool UGamePlatformTargetFrameWidget::BindTargetSource(FGuid InSourceScopeId)
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
    State = FGamePlatformUITargetFrameState();
    // 新观察源尚未确认目标可见，隐藏旧目标容器避免闪现或泄漏。
    SetVisibility(ESlateVisibility::Collapsed);
    ResetTargetVisuals();
    BP_OnTargetFrameChanged(State);
    return true;
}

bool UGamePlatformTargetFrameWidget::ApplyTargetSnapshot(
    const FGamePlatformUITargetFrameState& InState)
{
    if (!BoundScopeId.IsValid() ||
        InState.SourceScopeId != BoundScopeId ||
        !FGamePlatformUITargetFramePresentation::IsValidSnapshot(InState) ||
        InState.Revision <= State.Revision)
    {
        return false;
    }
    if (!InState.bVisible)
    {
        // 敌方迷雾或可见性撤销后立即清空所有可识别身份和状态，
        // 只保留不透明SourceScope与Revision以防迟到的旧显示事件复现。
        State = FGamePlatformUITargetFrameState();
        State.SourceScopeId = InState.SourceScopeId;
        State.Revision = InState.Revision;
        ResetTargetVisuals();
    }
    else
    {
        State = InState;
        if (TargetPortrait)
        {
            TargetPortrait->ApplyPortraitState(State.Portrait);
        }
        if (TargetHealthBar)
        {
            TargetHealthBar->ApplyResourceState(State.Health);
        }
        if (TargetShieldBar)
        {
            TargetShieldBar->ApplyResourceState(State.Shield);
        }
    }
    // 可见性只能由上游经授权快照确定，未授权/被迷雾隐藏时收起整个目标框。
    SetVisibility(State.bVisible
        ? ESlateVisibility::SelfHitTestInvisible
        : ESlateVisibility::Collapsed);
    BP_OnTargetFrameChanged(State);
    return true;
}

void UGamePlatformTargetFrameWidget::ClearTargetSource()
{
    if (!BoundScopeId.IsValid())
    {
        return;
    }
    BoundScopeId.Invalidate();
    State = FGamePlatformUITargetFrameState();
    SetVisibility(ESlateVisibility::Collapsed);
    ResetTargetVisuals();
    BP_OnTargetFrameChanged(State);
}

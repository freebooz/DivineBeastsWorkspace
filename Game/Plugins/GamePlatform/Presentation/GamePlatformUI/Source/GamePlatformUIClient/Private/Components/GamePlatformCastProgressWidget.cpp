// 平台客户端显示适配：事件只写一次快照，连续Percent插值交给UMG/MovieScene，不逐帧读取业务状态。
#include "Components/GamePlatformCastProgressWidget.h"

#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Animation/WidgetAnimation.h"
#include "MovieScene.h"
#include "Tracks/MovieSceneFloatTrack.h"
#include "Sections/MovieSceneFloatSection.h"

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
    const FGuid UpdateId = FGuid::NewGuid();
    DisplayUpdateId = UpdateId;
    StopCastVisualAnimation();
    // 停止动画允许用户蓝图同步重绑；旧绑定栈不能覆盖新作用域。
    if (DisplayUpdateId != UpdateId) return true;
    // 尚未收到可信施法快照时不展示空施法条。
    SetVisibility(ESlateVisibility::Collapsed);
    // SetVisibility同步广播可见性事件，监听者可以在本调用返回前发布有效后继快照。
    if (DisplayUpdateId != UpdateId) return true;
    if (CastProgressBar)
    {
        CastProgressBar->SetPercent(0.0f);
        if (DisplayUpdateId != UpdateId) return true;
    }
    if (CastNameText)
    {
        CastNameText->SetText(FText::GetEmpty());
        if (DisplayUpdateId != UpdateId) return true;
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
    const FGuid AppliedScope = BoundScopeId;
    const int64 AppliedRevision = State.Revision;
    const FGuid UpdateId = FGuid::NewGuid();
    DisplayUpdateId = UpdateId;
    StopCastVisualAnimation();
    if (DisplayUpdateId != UpdateId || BoundScopeId != AppliedScope || State.Revision != AppliedRevision) return true;
    if (CastProgressBar)
    {
        CastProgressBar->SetPercent(GetNormalizedCastProgress());
        if (DisplayUpdateId != UpdateId) return true;
    }
    if (CastNameText)
    {
        CastNameText->SetText(State.bActive ? State.DisplayName : FText::GetEmpty());
        if (DisplayUpdateId != UpdateId) return true;
    }
    // 客户端仅按经授权的bActive（当前施法状态）展开控件，不预测真正技能完成。
    SetVisibility(State.bActive
        ? ESlateVisibility::SelfHitTestInvisible
        : ESlateVisibility::Collapsed);
    // 可见性监听者可能已启动后继动画；旧发布栈必须在创建动画前退出，不能遗失内层动画所有权。
    if (DisplayUpdateId != UpdateId || BoundScopeId != AppliedScope || State.Revision != AppliedRevision) return true;
    PlayCastVisualAnimation();
    if (DisplayUpdateId != UpdateId || BoundScopeId != AppliedScope || State.Revision != AppliedRevision) return true;
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
    const FGuid UpdateId = FGuid::NewGuid();
    DisplayUpdateId = UpdateId;
    StopCastVisualAnimation();
    // 停止可同步触发后继绑定；清理旧来源不能清空新来源的文字/可见性。
    if (DisplayUpdateId != UpdateId) return;
    SetVisibility(ESlateVisibility::Collapsed);
    if (DisplayUpdateId != UpdateId) return;
    if (CastProgressBar)
    {
        CastProgressBar->SetPercent(0.0f);
        if (DisplayUpdateId != UpdateId) return;
    }
    if (CastNameText)
    {
        CastNameText->SetText(FText::GetEmpty());
        if (DisplayUpdateId != UpdateId) return;
    }
    BP_OnCastStateChanged(State);
}

void UGamePlatformCastProgressWidget::StopCastVisualAnimation()
{
    UWidgetAnimation* Previous = CastVisualAnimation;
    CastVisualAnimation = nullptr;
    if (Previous) StopAnimation(Previous);
}

void UGamePlatformCastProgressWidget::PlayCastVisualAnimation()
{
    if (!CastProgressBar || !State.bActive || State.TotalSeconds <= KINDA_SMALL_NUMBER ||
        State.RemainingSeconds <= KINDA_SMALL_NUMBER) return;
    // 毫秒刻度覆盖既有7天视觉上限且不溢出int32；曲线仅消耗此事件已知的时间，不回读业务。
    constexpr int32 VisualFramesPerSecond = 1000;
    const int32 DurationFrames = FMath::Max(1, FMath::CeilToInt(State.RemainingSeconds * VisualFramesPerSecond));
    UWidgetAnimation* Animation = NewObject<UWidgetAnimation>(this, NAME_None, RF_Transient);
    UMovieScene* Scene = NewObject<UMovieScene>(Animation, NAME_None, RF_Transient);
    Animation->MovieScene = Scene;
    Scene->SetTickResolutionDirectly(FFrameRate(VisualFramesPerSecond, 1));
    Scene->SetDisplayRate(FFrameRate(60, 1));
    Scene->SetPlaybackRange(0, DurationFrames);
    const FGuid BindingId = Scene->AddPossessable(CastProgressBar->GetName(), UProgressBar::StaticClass());
    Animation->BindPossessableObject(BindingId, *CastProgressBar, this);
    UMovieSceneFloatTrack* Track = Scene->AddTrack<UMovieSceneFloatTrack>(BindingId);
    Track->SetPropertyNameAndPath(TEXT("Percent"), TEXT("Percent"));
    auto* Section = Cast<UMovieSceneFloatSection>(Track->CreateNewSection());
    Section->SetRange(TRange<FFrameNumber>(FFrameNumber(0), FFrameNumber(DurationFrames + 1)));
    // 完成后停在末值，直到真实结束/取消/重同步事件；完成动画不授予或撤销任何技能资格。
    Section->SetCompletionMode(EMovieSceneCompletionMode::KeepState);
    Section->GetChannel().AddLinearKey(FFrameNumber(0), GetNormalizedCastProgress());
    Section->GetChannel().AddLinearKey(FFrameNumber(DurationFrames), 1.0f);
    Track->AddSection(*Section);
    CastVisualAnimation = Animation;
    PlayAnimation(Animation, 0.0f, 1, EUMGSequencePlayMode::Forward, 1.0f, false);
}

void UGamePlatformCastProgressWidget::NativeDestruct()
{
    ClearCastSource();
    Super::NativeDestruct();
}

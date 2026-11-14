// 项目层事件驱动加载页：只展示当前Loading VM快照；委托/激活/换VM所有权统一归平台基类。
// 不持有加载事务权威、不伪造进度；阶段文本、进度条与百分比绑定真实资产，本文件不生成或改资产。
#include "Screens/Loading/DivineBeastsLoadingTravelScreen.h"

#include "ViewModels/Loading/DivineBeastsLoadingViewModel.h"
#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

UDivineBeastsLoadingViewModel*
UDivineBeastsLoadingTravelScreen::GetLoadingViewModel() const
{
    // 使用平台 Screen 已持有的唯一 ViewModel，不复制加载状态所有权。
    return Cast<UDivineBeastsLoadingViewModel>(GetViewModel());
}

void UDivineBeastsLoadingTravelScreen::RefreshInitialState()
{
    // 平台先验证激活/VM代次，替换VM也会经同一钩子刷新；不在Super激活通知返回后另建订阅。
    Super::RefreshInitialState();
    if (const auto* LocalViewModel = GetLoadingViewModel())
        RefreshLoadingPresentation(LocalViewModel->GetRevision(), LocalViewModel->GetPageGeneration());
}

void UDivineBeastsLoadingTravelScreen::OnViewModelStateChanged(int32 Revision, int32 PageGeneration)
{
    // 动态委托仅由平台基类持有；合法换VM精确拆旧/接新，派生只消费过滤后的快照。
    Super::OnViewModelStateChanged(Revision, PageGeneration);
    RefreshLoadingPresentation(Revision, PageGeneration);
}

void UDivineBeastsLoadingTravelScreen::RefreshLoadingPresentation(int32, int32 PageGeneration)
{
    auto* LocalViewModel = GetLoadingViewModel();
    // 二次复核可见页面与捕获代次；失活/过期通知不能把文本恢复到旧页面。
    if (!IsActivated() || !StageText || !LocalViewModel || !LocalViewModel->IsPageActive() ||
        LocalViewModel->GetPageGeneration() != PageGeneration) return;
    const auto Snapshot = LocalViewModel->GetLoadingSnapshot();
    StopCountdown();
    UWorld* World = GetWorld();
    const bool bShowWorld = Snapshot.Presentation.bShowWorld && World && !World->bIsTearingDown;
    SetWorldBackdrop(bShowWorld);
    StageText->SetText(Snapshot.Stage.IsEmpty()
        ? NSLOCTEXT("DivineBeastsUI", "PreparingWorld", "正在准备世界，请稍候") : Snapshot.Stage);
    if (bShowWorld && Snapshot.Presentation.CountdownDeadlineServerTime > 0.0)
    {
        if (AGameStateBase* GameState = World->GetGameState())
        {
            CountdownStage = Snapshot.Stage;
            CountdownWorld = World;
            CountdownClock = GameState;
            CountdownDeadlineServerTime = Snapshot.Presentation.CountdownDeadlineServerTime;
            UpdateCountdownText();
        }
    }
    const bool bKnown=FMath::IsFinite(Snapshot.Progress)&&Snapshot.Progress>=0.0f;
    if(LoadProgress){LoadProgress->SetIsMarquee(!bKnown);LoadProgress->SetPercent(bKnown?FMath::Clamp(Snapshot.Progress,0.0f,1.0f):0.0f);}
    if(ProgressPercentText)
    {
        FNumberFormattingOptions Format;Format.MinimumFractionalDigits=0;Format.MaximumFractionalDigits=0;
        ProgressPercentText->SetText(bKnown?FText::AsPercent(FMath::Clamp(Snapshot.Progress,0.0f,1.0f),&Format):
            NSLOCTEXT("DivineBeastsUI","WorldProgressUnknown","正在连接，请稍候"));
    }
}

void UDivineBeastsLoadingTravelScreen::NativeOnDeactivated()
{
    StopCountdown();
    SetWorldBackdrop(false);
    Super::NativeOnDeactivated();
}

void UDivineBeastsLoadingTravelScreen::NativeDestruct()
{
    StopCountdown();
    SetWorldBackdrop(false);
    Super::NativeDestruct();
}

void UDivineBeastsLoadingTravelScreen::SetWorldBackdrop(bool bShowWorld)
{
    // 复用现有蓝图的背景与遮罩，保留阶段、进度与 UIOnly 输入层。
    if (UWidget* Scene = GetWidgetFromName(TEXT("SceneBackdrop")))
    {
        if (!InitialSceneOpacity.IsSet()) InitialSceneOpacity = Scene->GetRenderOpacity();
        Scene->SetRenderOpacity(bShowWorld ? 0.0f : InitialSceneOpacity.GetValue());
    }
    if (UWidget* Shade = GetWidgetFromName(TEXT("BackdropShade")))
    {
        if (!InitialShadeOpacity.IsSet()) InitialShadeOpacity = Shade->GetRenderOpacity();
        const float Opacity = InitialShadeOpacity.GetValue();
        Shade->SetRenderOpacity(bShowWorld ? Opacity * 0.45f : Opacity);
    }
}

void UDivineBeastsLoadingTravelScreen::UpdateCountdownText()
{
    UWorld* World = CountdownWorld.Get();
    const AGameStateBase* Clock = CountdownClock.Get();
    if (!IsActivated() || !StageText || !World || World != GetWorld() || World->bIsTearingDown ||
        !Clock || World->GetGameState() != Clock)
    {
        StopCountdown();
        return;
    }
    // 每次数字重绘只读时钟，以吸收引擎接收的服务器时间修正。
    const double Remaining = CountdownDeadlineServerTime - Clock->GetServerWorldTimeSeconds();
    if (!FMath::IsFinite(Remaining) || Remaining >= static_cast<double>(MAX_int32))
    {
        StopCountdown();
        return;
    }
    if (Remaining <= 0.0)
    {
        StageText->SetText(NSLOCTEXT("DivineBeastsUI", "WaitingForBattleStart", "等待服务器开战确认"));
        StopCountdown();
        return;
    }
    const int32 Seconds = FMath::CeilToInt32(Remaining);
    StageText->SetText(FText::Format(
        NSLOCTEXT("DivineBeastsUI", "LoadingCountdownSeconds", "{0} · {1} 秒"),
        CountdownStage, FText::AsNumber(Seconds)));
    const float UntilNextSecond = static_cast<float>(FMath::Max(0.01, Remaining - (Seconds - 1)));
    // 仅为当前可见数字安排下一次重绘；不向网络或业务层请求新状态。
    World->GetTimerManager().SetTimer(CountdownTimer, this, &ThisClass::UpdateCountdownText, UntilNextSecond, false);
}

void UDivineBeastsLoadingTravelScreen::StopCountdown()
{
    if (UWorld* World = CountdownWorld.Get()) World->GetTimerManager().ClearTimer(CountdownTimer);
    CountdownTimer.Invalidate();
    CountdownWorld.Reset();
    CountdownClock.Reset();
    CountdownDeadlineServerTime = 0.0;
    CountdownStage = FText::GetEmpty();
}

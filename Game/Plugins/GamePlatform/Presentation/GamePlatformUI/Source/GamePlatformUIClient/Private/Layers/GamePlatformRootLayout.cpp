// 平台层客户端根布局：可选性能文本消费引擎帧结束通知，不读取账号、角色或战斗业务。
// 第三层通过Monolith提供FrameRateText的字体、颜色及位置；本文件不创建视觉资产。
// 通知属于Widget作用域，Construct/Destruct成对绑定，禁止Widget Tick或定时器查询状态。
#include "Layers/GamePlatformRootLayout.h"

#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Parse.h"

void UGamePlatformRootLayout::NativeConstruct()
{
    Super::NativeConstruct();
    UnbindFrameRateNotifications();
    FrameRateText = Cast<UTextBlock>(GetWidgetFromName(TEXT("FrameRateText")));
    if (!FrameRateText.IsValid()) return; // 没有此可选控件的其他项目继续沿用原根布局。
    const bool bShowFrameRate = !IsDesignTime() &&
        FParse::Param(FCommandLine::Get(), TEXT("GamePlatformShowFPS"));
    FrameRateText->SetVisibility(bShowFrameRate
        ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (!bShowFrameRate) return;
    FrameRateText->SetText(NSLOCTEXT("GamePlatformUI", "FrameRatePending", "FPS: --"));
    FrameRateSampleStartSeconds = FPlatformTime::Seconds();
    FrameRateEndFrameHandle = FCoreDelegates::OnEndFrame.AddUObject(
        this, &ThisClass::HandleFrameRateEndFrame);
}

void UGamePlatformRootLayout::NativeDestruct()
{
    UnbindFrameRateNotifications();
    Super::NativeDestruct();
}

void UGamePlatformRootLayout::UnbindFrameRateNotifications()
{
    if (FrameRateEndFrameHandle.IsValid())
    {
        FCoreDelegates::OnEndFrame.Remove(FrameRateEndFrameHandle);
        FrameRateEndFrameHandle.Reset();
    }
    FrameRateText.Reset();
    FrameRateSampleFrames = 0;
    LastDisplayedFrameRate = INDEX_NONE;
}

void UGamePlatformRootLayout::HandleFrameRateEndFrame()
{
    if (!FrameRateText.IsValid()) return;
    ++FrameRateSampleFrames;
    const double NowSeconds = FPlatformTime::Seconds();
    const double ElapsedSeconds = NowSeconds - FrameRateSampleStartSeconds;
    if (ElapsedSeconds < 0.25) return;
    // 使用真实经过时间，避免固定游戏DeltaSeconds、时间缩放或技能动画影响FPS读数。
    const int32 FramesPerSecond = FMath::RoundToInt(FrameRateSampleFrames / ElapsedSeconds);
    FrameRateSampleFrames = 0;
    FrameRateSampleStartSeconds = NowSeconds;
    if (FramesPerSecond == LastDisplayedFrameRate) return;
    LastDisplayedFrameRate = FramesPerSecond;
    FrameRateText->SetText(FText::Format(
        NSLOCTEXT("GamePlatformUI", "FramesPerSecond", "FPS: {0}"),
        FText::AsNumber(FramesPerSecond)));
}

#include "Screens/GamePlatformLoadingScreen.h"

UGamePlatformLoadingScreen::UGamePlatformLoadingScreen(
    const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // 加载页面由真实 Loading Token 生命周期控制，默认不允许 Back 主动关闭。
    bAllowBack = false;
}

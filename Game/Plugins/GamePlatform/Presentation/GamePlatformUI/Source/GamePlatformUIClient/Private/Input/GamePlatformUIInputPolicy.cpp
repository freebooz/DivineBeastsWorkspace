// 平台输入配置：仅生成CommonUI原生配置，不持有玩家权威、不创建第二套输入路由。
#include "Input/GamePlatformUIInputPolicy.h"
#include "CommonInputModeTypes.h"
#include "Engine/World.h"

FUIInputConfig UGamePlatformUIInputPolicy::BuildInputConfig(EGamePlatformUIInputMode InputMode)
{
    switch (InputMode)
    {
    case EGamePlatformUIInputMode::GameOnly:
    {
        FUIInputConfig Config(
            ECommonInputMode::Game,
            EMouseCaptureMode::CapturePermanently,
            true);
        Config.bIgnoreMoveInput = false;
        Config.bIgnoreLookInput = false;
        return Config;
    }
    case EGamePlatformUIInputMode::UIOnly:
    {
        FUIInputConfig Config(
            ECommonInputMode::Menu,
            EMouseCaptureMode::NoCapture,
            false);
        Config.bIgnoreMoveInput = true;
        Config.bIgnoreLookInput = true;
        return Config;
    }
    case EGamePlatformUIInputMode::GameAndUI:
    default:
    {
        FUIInputConfig Config(
            ECommonInputMode::All,
            // NoCapture下游戏视口会丢弃尚未捕获时的首次鼠标按下；
            // 按下期间临时捕获沿用原生GameAndUI语义，UI仍先处理，松开后恢复光标。
            EMouseCaptureMode::CaptureDuringMouseDown,
            false);
        Config.bIgnoreMoveInput = false;
        Config.bIgnoreLookInput = false;
        return Config;
    }
    }
}

bool UGamePlatformUIInputPolicy::CanPauseWorld(
    const UWorld* World,
    EGamePlatformUIPausePolicy PausePolicy)
{
    return PausePolicy == EGamePlatformUIPausePolicy::StandaloneOnly &&
           IsValid(World) &&
           World->GetNetMode() == NM_Standalone;
}

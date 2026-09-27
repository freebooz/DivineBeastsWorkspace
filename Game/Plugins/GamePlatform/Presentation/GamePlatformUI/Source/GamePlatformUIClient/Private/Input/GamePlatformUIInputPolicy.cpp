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
            EMouseCaptureMode::NoCapture,
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

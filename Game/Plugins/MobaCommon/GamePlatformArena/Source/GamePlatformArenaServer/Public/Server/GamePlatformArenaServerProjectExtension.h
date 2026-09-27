#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "Arena/GamePlatformArenaTypes.h"
#include "Definitions/GamePlatformArenaModeDefinition.h"

class AGamePlatformArenaGameMode;

/**
 * IGamePlatformArenaServerProjectExtension（竞技服务器项目扩展接口）。
 * MobaCommon只认识此中立接口；具体项目实现由上层插件通过Modular Feature注册。
 */
class GAMEPLATFORMARENASERVER_API IGamePlatformArenaServerProjectExtension
    : public IModularFeature
{
public:
    virtual ~IGamePlatformArenaServerProjectExtension() = default;

    static FName GetModularFeatureName()
    {
        static const FName Name(TEXT("GamePlatform.Arena.ServerProjectExtension"));
        return Name;
    }

    /** Assignment应用前解析项目认可的最终ModeSpec；失败则服务器Fail Closed。 */
    virtual bool ResolveModeSpec(
        const FGamePlatformArenaAssignment& Assignment,
        FGamePlatformArenaModeSpec& OutModeSpec,
        FString& OutError) const = 0;

    /** 校验项目Revision/Map/Content/Hero Catalog等并注入合法项目Policy Provider。 */
    virtual bool ValidateAndConfigureGameMode(
        AGamePlatformArenaGameMode& GameMode,
        const FGamePlatformArenaAssignment& Assignment,
        const FGamePlatformArenaModeSpec& ModeSpec,
        FString& OutError) = 0;
};

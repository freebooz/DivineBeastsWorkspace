#pragma once

#include "CoreMinimal.h"
#include "Arena/GamePlatformArenaPolicies.h"
#include "Server/GamePlatformArenaServerProjectExtension.h"

/**
 * FDivineBeastsArenaServerProjectExtension（神兽联盟竞技服务器项目扩展）。
 * 负责Production项目配置门禁与Hero资格；不重写平台GameMode生命周期。
 */
class DIVINEBEASTSARENASERVER_API FDivineBeastsArenaServerProjectExtension final
    : public IGamePlatformArenaServerProjectExtension
    , public IGamePlatformArenaHeroEligibilityProvider
{
public:
    virtual bool ResolveModeSpec(
        const FGamePlatformArenaAssignment& Assignment,
        FGamePlatformArenaModeSpec& OutModeSpec,
        FString& OutError) const override;

    virtual bool ValidateAndConfigureGameMode(
        AGamePlatformArenaGameMode& GameMode,
        const FGamePlatformArenaAssignment& Assignment,
        const FGamePlatformArenaModeSpec& ModeSpec,
        FString& OutError) override;

    virtual bool IsHeroEligible(
        const FString& PlayerId,
        const FString& HeroDefinitionId,
        FName ArenaModeId,
        FString& OutReason) const override;
};

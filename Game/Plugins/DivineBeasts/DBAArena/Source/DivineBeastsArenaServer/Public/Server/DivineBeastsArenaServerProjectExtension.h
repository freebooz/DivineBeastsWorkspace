#pragma once

#include "CoreMinimal.h"
#include "Arena/GamePlatformArenaPolicies.h"
#include "Server/GamePlatformArenaServerProjectExtension.h"

struct FStreamableHandle;

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
private:
    /** 当前MainArena世界唯一玩法生命周期适配器；GameMode仅借用原始接口指针。 */
    TSharedPtr<IGamePlatformArenaGameplayLifecycleAdapter> GameplayLifecycleAdapter;

    /** Server-safe Hero Definition预热租约；比赛结束/新Assignment覆盖时随扩展生命周期释放。 */
    TArray<TSharedPtr<FStreamableHandle>> HeroDefinitionWarmupLeases;
};

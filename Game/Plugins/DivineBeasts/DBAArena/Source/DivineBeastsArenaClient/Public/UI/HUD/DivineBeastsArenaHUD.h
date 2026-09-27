#pragma once

#include "UI/GamePlatformMobaArenaHUDBase.h"
#include "DivineBeastsArenaHUD.generated.h"

/**
 * UDivineBeastsArenaHUD（神兽联盟竞技HUD基类）。
 *
 * 归属可选 DBAArena 插件，而不是公共 DBAClient。
 * PC与Mobile可使用不同Widget Blueprint，但共享同一 UGamePlatformArenaViewModel。
 */
UCLASS(Abstract, Blueprintable)
class DIVINEBEASTSARENACLIENT_API UDivineBeastsArenaHUD
    : public UGamePlatformMobaArenaHUDBase
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Arena|UI")
    FName GetHUDSurfaceId() const
    {
        return TEXT("UI.HUD.Arena");
    }
};

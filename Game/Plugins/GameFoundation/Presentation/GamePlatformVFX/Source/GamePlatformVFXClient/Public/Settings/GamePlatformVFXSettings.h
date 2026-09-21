#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "GamePlatformVFXSettings.generated.h"

UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Game Platform VFX / 游戏平台视觉特效"))
class GAMEPLATFORMVFXCLIENT_API UGamePlatformVFXSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(Config, EditAnywhere, Category="Budget", meta=(ClampMin="1"))
    int32 MaxActiveInstances = 512;

    UPROPERTY(Config, EditAnywhere, Category="Budget", meta=(ClampMin="0"))
    int32 MaxAmbientInstances = 192;

    UPROPERTY(Config, EditAnywhere, Category="Diagnostics")
    bool bEnableDiagnostics = true;

    UPROPERTY(Config, EditAnywhere, Category="Fallback")
    bool bAllowPlatformFallback = true;
};

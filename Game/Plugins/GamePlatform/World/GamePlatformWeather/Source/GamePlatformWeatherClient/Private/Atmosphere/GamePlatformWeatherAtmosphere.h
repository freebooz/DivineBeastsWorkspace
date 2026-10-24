// WeatherClient私有光雾适配器：基线只捕获一次，使用已有快照插值，不创建第二个天气服务。
#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformWeatherAtmosphereProfile.h"
#include "Types/GamePlatformWeatherTypes.h"
class UWorld;
class ULightComponentBase;
class UExponentialHeightFogComponent;
class AExponentialHeightFog;
class AActor;
class ULevel;

class FGamePlatformWeatherAtmosphere
{
public:
    FGamePlatformWeatherAtmosphere(UWorld& InWorld, const FGamePlatformWeatherAtmosphereProfile& InProfile);
    ~FGamePlatformWeatherAtmosphere();
    /** 主线程输入已经插值的0..1阴云/雾权重；不更改权威天气。 */
    void Apply(float CloudWeight, float FogWeight);
    /** 从天气快照端点计算阴云权重，调用方在端点之间连续插值，避免Type中点切换跳光。 */
    static float CloudWeight(const FGamePlatformWeatherState& State);
private:
    void CaptureActor(AActor* Actor);
    void OnLevelAdded(ULevel* Level, UWorld* AddedWorld);
    void OnLevelRemoved(ULevel* Level, UWorld* RemovedWorld);
    void EnsureLocalFog();
    struct FLightBaseline
    {
        TWeakObjectPtr<ULightComponentBase> Component;
        float Intensity = 0.f;
        FLinearColor Color;
        bool bSky = false;
    };
    struct FFogBaseline
    {
        TWeakObjectPtr<UExponentialHeightFogComponent> Component;
        float Density = 0.f;
        float Start = 0.f;
        FLinearColor Color;
    };
    TWeakObjectPtr<UWorld> World;
    FGamePlatformWeatherAtmosphereProfile Profile;
    TArray<FLightBaseline> Lights;
    TArray<FFogBaseline> Fogs;
    TWeakObjectPtr<AExponentialHeightFog> OwnedFog;
    FDelegateHandle LevelAddedHandle;
    FDelegateHandle LevelRemovedHandle;
    float LastCloud = -1.f;
    float LastFog = -1.f;
    float LastLoggedCloud = -1.f;
    float LastLoggedFog = -1.f;
};

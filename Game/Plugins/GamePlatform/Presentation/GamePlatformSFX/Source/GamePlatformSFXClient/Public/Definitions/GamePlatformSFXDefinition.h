#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformSFXTypes.h"
#include "GamePlatformSFXDefinition.generated.h"

class USoundAttenuation;
class USoundBase;
class USoundConcurrency;

/**
 * UGamePlatformSFXDefinition（游戏平台音效定义）。
 * 只描述跨游戏音效执行所需数据；具体声音资产由项目/内容包实例提供。
 * SoundBase可指向SoundWave、SoundCue或MetaSound Source，不在平台层复制这些引擎类型。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMSFXCLIENT_API UGamePlatformSFXDefinition : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()

public:
    virtual FGamePlatformResult ValidateDefinition() const override;

    const TSoftObjectPtr<USoundBase>& GetSound() const { return Sound; }
    const TSoftObjectPtr<USoundAttenuation>& GetAttenuation() const { return Attenuation; }
    const TSoftObjectPtr<USoundConcurrency>& GetConcurrency() const { return Concurrency; }
    EGamePlatformSFXPlaybackSpace GetPlaybackSpace() const { return PlaybackSpace; }
    bool ShouldStopWhenOwnerDestroyed() const { return bStopWhenOwnerDestroyed; }
    float GetVolumeMultiplier() const { return VolumeMultiplier; }
    float GetPitchMultiplier() const { return PitchMultiplier; }
    float GetFadeInSeconds() const { return FadeInSeconds; }
    float GetFadeOutSeconds() const { return FadeOutSeconds; }
    const TSet<FName>& GetAllowedFloatParameters() const { return AllowedFloatParameters; }
    const TMap<FName, float>& GetDefaultFloatParameters() const { return DefaultFloatParameters; }

    bool IsFloatParameterAllowed(FName Name) const
    {
        return !Name.IsNone() && AllowedFloatParameters.Contains(Name);
    }

protected:
    /** Runtime Bundle确保Definition租约同时持有真实声音资源。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Assets", meta=(AssetBundles="SFXRuntime"))
    TSoftObjectPtr<USoundBase> Sound;

    /** 为空时使用Sound自身/引擎默认衰减。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Spatial", meta=(AssetBundles="SFXRuntime"))
    TSoftObjectPtr<USoundAttenuation> Attenuation;

    /** 为空时使用Sound自身并发策略；平台不复制SoundConcurrency机制。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Concurrency", meta=(AssetBundles="SFXRuntime"))
    TSoftObjectPtr<USoundConcurrency> Concurrency;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Playback")
    EGamePlatformSFXPlaybackSpace PlaybackSpace = EGamePlatformSFXPlaybackSpace::World;

    /** Attached模式下Owner销毁时是否同步停止声音。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Playback")
    bool bStopWhenOwnerDestroyed = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Playback", meta=(ClampMin="0.0", ClampMax="4.0"))
    float VolumeMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Playback", meta=(ClampMin="0.25", ClampMax="4.0"))
    float PitchMultiplier = 1.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Fade", meta=(ClampMin="0.0", ClampMax="10.0"))
    float FadeInSeconds = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Fade", meta=(ClampMin="0.0", ClampMax="10.0"))
    float FadeOutSeconds = 0.0f;

    /** 请求可覆盖的浮点参数白名单；避免任意字符串参数污染MetaSound实例。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Parameters")
    TSet<FName> AllowedFloatParameters;

    /** 每次实例创建后先应用默认参数，再应用请求级覆盖。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SFX|Parameters")
    TMap<FName, float> DefaultFloatParameters;
};

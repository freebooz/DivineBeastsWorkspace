// GamePlatformSFX拥有的设备本地三路用户音量；仅客户端混音，不参与服务器权威和网络复制。
#pragma once
#include "Types/GamePlatformSFXTypes.h"
#include "GamePlatformSFXUserSettingsTypes.generated.h"

/** 全量只读投影，音量为0..1线性增益；Revision仅当前GameInstance有效，不能跨实例比较。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMSFXCLIENT_API FGamePlatformSFXUserVolumes
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|UserSettings") float Effects = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|UserSettings") float Ambient = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|UserSettings") float Music = 1.0f;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="SFX|UserSettings") int64 Revision = 1;

    /** 未知分组保守使用效果音，不从播放空间或资产名字猜测分类。 */
    float GetGain(EGamePlatformSFXSoundGroup Group) const
    {
        switch (Group)
        {
        case EGamePlatformSFXSoundGroup::Ambient: return Ambient;
        case EGamePlatformSFXSoundGroup::Music: return Music;
        default: return Effects;
        }
    }
};
DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformSFXUserVolumesChanged, const FGamePlatformSFXUserVolumes&);

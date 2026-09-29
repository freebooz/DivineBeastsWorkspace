#pragma once

#include "GameFramework/SaveGame.h"
#include "Types/GamePlatformSettingTypes.h"
#include "GamePlatformUserSettingsProfile.generated.h"

/**
 * 客户端本地用户设置档案。
 * 只保存 GamePlatformSettings Runtime 的 User 层纯值，不保存账号认证、角色、背包、装备或服务器状态。
 */
UCLASS()
class GAMEPLATFORMSETTINGSCLIENT_API UGamePlatformUserSettingsProfile
    : public USaveGame
{
    GENERATED_BODY()

public:
    /** 档案Schema版本；Runtime通过Migration逐版本升级，失败时保留原文件不覆盖。 */
    UPROPERTY()
    int32 SchemaVersion = 1;

    /** 类型化User层值；SettingId由Runtime Registry再次校验。 */
    UPROPERTY()
    TMap<FName, FGamePlatformSettingValue> UserValues;
};

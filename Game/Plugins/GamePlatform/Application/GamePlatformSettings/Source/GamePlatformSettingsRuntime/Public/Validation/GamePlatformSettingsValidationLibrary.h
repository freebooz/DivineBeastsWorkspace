#pragma once

#include "Types/GamePlatformSettingTypes.h"

/**
 * 公共只读验证入口。
 * Editor/CI 可在不创建 GameInstance 的情况下校验当前已注册 Provider 的冲突与Descriptor合法性。
 */
struct GAMEPLATFORMSETTINGSRUNTIME_API FGamePlatformSettingsValidationLibrary final
{
    static FGamePlatformResult ValidateRegisteredProviders(
        EGamePlatformSettingRuntimeScope RuntimeScope,
        int32 MaxProviders,
        int32 MaxDescriptors);
};

#include "Validation/GamePlatformSettingsEditorValidator.h"

#include "Settings/GamePlatformSettingsProjectSettings.h"
#include "Validation/GamePlatformSettingsValidationLibrary.h"

bool FGamePlatformSettingsEditorValidator::ValidateAll(
    TArray<FString>& OutErrors)
{
    OutErrors.Reset();

    const UGamePlatformSettingsProjectSettings* Settings =
        GetDefault<UGamePlatformSettingsProjectSettings>();
    const int32 MaxProviders = Settings ? Settings->MaxProviders : 64;
    const int32 MaxDescriptors = Settings ? Settings->MaxDescriptors : 1024;

    for (const EGamePlatformSettingRuntimeScope RuntimeScope :
        { EGamePlatformSettingRuntimeScope::Client,
          EGamePlatformSettingRuntimeScope::Server })
    {
        const FGamePlatformResult Result =
            FGamePlatformSettingsValidationLibrary::
                ValidateRegisteredProviders(
                    RuntimeScope,
                    MaxProviders,
                    MaxDescriptors);
        if (!Result.IsSuccess())
        {
            OutErrors.Add(
                FString::Printf(
                    TEXT("%s端设置Provider验证失败：%s / %s"),
                    RuntimeScope == EGamePlatformSettingRuntimeScope::Client
                        ? TEXT("Client")
                        : TEXT("Server"),
                    *Result.Code.ToString(),
                    *Result.Message));
        }
    }

    return OutErrors.IsEmpty();
}

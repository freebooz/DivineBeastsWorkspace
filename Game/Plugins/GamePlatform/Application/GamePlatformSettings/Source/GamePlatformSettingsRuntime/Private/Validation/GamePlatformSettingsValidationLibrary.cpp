#include "Validation/GamePlatformSettingsValidationLibrary.h"

#include "Registry/GamePlatformSettingsRegistry.h"

FGamePlatformResult
FGamePlatformSettingsValidationLibrary::ValidateRegisteredProviders(
    const EGamePlatformSettingRuntimeScope RuntimeScope,
    const int32 MaxProviders,
    const int32 MaxDescriptors)
{
    FGamePlatformSettingsRegistry Registry;
    return Registry.Rebuild(
        MaxProviders,
        MaxDescriptors,
        RuntimeScope);
}

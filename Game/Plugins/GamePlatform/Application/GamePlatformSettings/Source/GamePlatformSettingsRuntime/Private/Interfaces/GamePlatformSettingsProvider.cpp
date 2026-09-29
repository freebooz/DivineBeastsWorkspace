#include "Interfaces/IGamePlatformSettingsProvider.h"

FName IGamePlatformSettingsProvider::GetModularFeatureName()
{
    return FName(TEXT("GamePlatformSettingsProvider"));
}

FName IGamePlatformSettingsPersistenceProvider::GetModularFeatureName()
{
    return FName(TEXT("GamePlatformSettingsPersistenceProvider"));
}

FName IGamePlatformSettingsMigration::GetModularFeatureName()
{
    return FName(TEXT("GamePlatformSettingsMigration"));
}

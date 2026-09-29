#include "Interfaces/IGamePlatformDeviceSettingsService.h"

#include "Engine/GameInstance.h"
#include "Subsystems/GamePlatformDeviceSettingsSubsystem.h"

IGamePlatformDeviceSettingsService* IGamePlatformDeviceSettingsService::Get(
    UGameInstance& GameInstance)
{
    return GameInstance.GetSubsystem<UGamePlatformDeviceSettingsSubsystem>();
}

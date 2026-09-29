#include "Interfaces/IGamePlatformSettingsService.h"

#include "Engine/GameInstance.h"
#include "Subsystems/GamePlatformSettingsSubsystem.h"

IGamePlatformSettingsService* IGamePlatformSettingsService::Get(
    UGameInstance& GameInstance)
{
    return GameInstance.GetSubsystem<UGamePlatformSettingsSubsystem>();
}

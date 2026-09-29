#include "Interfaces/IGamePlatformSaveService.h"

#include "Engine/GameInstance.h"
#include "Subsystems/GamePlatformSaveSubsystem.h"

IGamePlatformSaveService* IGamePlatformSaveService::Get(
    UGameInstance& GameInstance)
{
    return GameInstance.GetSubsystem<UGamePlatformSaveSubsystem>();
}

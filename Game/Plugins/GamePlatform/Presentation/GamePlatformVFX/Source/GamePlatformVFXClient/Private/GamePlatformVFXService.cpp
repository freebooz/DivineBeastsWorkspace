#include "Interfaces/GamePlatformVFXService.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Engine/World.h"

IGamePlatformVFXService* IGamePlatformVFXService::Get(UWorld* World)
{
    if (!IsValid(World) || World->GetNetMode() == NM_DedicatedServer)
    {
        return nullptr;
    }

    return World->GetSubsystem<UGamePlatformVFXWorldSubsystem>();
}

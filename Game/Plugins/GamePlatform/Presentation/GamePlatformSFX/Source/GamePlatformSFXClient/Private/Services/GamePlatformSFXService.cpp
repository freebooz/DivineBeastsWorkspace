#include "Interfaces/IGamePlatformSFXService.h"

#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Subsystems/GamePlatformSFXWorldSubsystem.h"

IGamePlatformSFXService* IGamePlatformSFXService::Get(const UObject* WorldContextObject)
{
    if (!WorldContextObject || !GEngine)
    {
        return nullptr;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(
        WorldContextObject,
        EGetWorldErrorMode::ReturnNull);
    return World ? World->GetSubsystem<UGamePlatformSFXWorldSubsystem>() : nullptr;
}

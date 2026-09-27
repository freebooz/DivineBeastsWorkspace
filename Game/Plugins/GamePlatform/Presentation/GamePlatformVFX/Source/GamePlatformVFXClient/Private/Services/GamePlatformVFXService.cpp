#include "Interfaces/GamePlatformVFXService.h"
#include "Engine/Engine.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"

IGamePlatformVFXService* IGamePlatformVFXService::Get(const UObject* WorldContextObject)
{
    if (!WorldContextObject || !GEngine)
    {
        return nullptr;
    }

    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull);
    return World ? World->GetSubsystem<UGamePlatformVFXWorldSubsystem>() : nullptr;
}

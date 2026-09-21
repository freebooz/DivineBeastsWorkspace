#include "Diagnostics/GamePlatformVFXDiagnostics.h"
#include "Settings/GamePlatformVFXSettings.h"

DEFINE_LOG_CATEGORY(LogGamePlatformVFX);

void FGamePlatformVFXDiagnostics::Info(const FString& Message)
{
    if (const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>(); Settings && Settings->bEnableDiagnostics)
    {
        UE_LOG(LogGamePlatformVFX, Log, TEXT("%s"), *Message);
    }
}

void FGamePlatformVFXDiagnostics::Warning(const FString& Message)
{
    UE_LOG(LogGamePlatformVFX, Warning, TEXT("%s"), *Message);
}

void FGamePlatformVFXDiagnostics::Error(const FString& Message)
{
    UE_LOG(LogGamePlatformVFX, Error, TEXT("%s"), *Message);
}

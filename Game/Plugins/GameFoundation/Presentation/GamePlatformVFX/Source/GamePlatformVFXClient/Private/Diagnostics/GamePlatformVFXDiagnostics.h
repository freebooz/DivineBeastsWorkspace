#pragma once
#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGamePlatformVFX, Log, All);

class FGamePlatformVFXDiagnostics
{
public:
    static void Info(const FString& Message);
    static void Warning(const FString& Message);
    static void Error(const FString& Message);
};

#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogGamePlatformDebug, Log, All);

namespace GamePlatformDebugPrivate
{
    void RegisterBuiltInProviders();
    void UnregisterBuiltInProviders();

    void RegisterConsoleCommands();
    void UnregisterConsoleCommands();

    void RegisterGameplayDebuggerCategories();
    void UnregisterGameplayDebuggerCategories();
}

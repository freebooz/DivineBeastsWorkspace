#include "GamePlatformDebugPrivate.h"

#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogGamePlatformDebug);

class FGamePlatformDebugModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
#if !UE_BUILD_SHIPPING
        GamePlatformDebugPrivate::RegisterBuiltInProviders();
        GamePlatformDebugPrivate::RegisterConsoleCommands();
        GamePlatformDebugPrivate::RegisterGameplayDebuggerCategories();

        UE_LOG(
            LogGamePlatformDebug,
            Log,
            TEXT("GamePlatformDebug started in non-Shipping configuration."));
#endif
    }

    virtual void ShutdownModule() override
    {
#if !UE_BUILD_SHIPPING
        GamePlatformDebugPrivate::UnregisterGameplayDebuggerCategories();
        GamePlatformDebugPrivate::UnregisterConsoleCommands();
        GamePlatformDebugPrivate::UnregisterBuiltInProviders();
#endif
    }
};

IMPLEMENT_MODULE(FGamePlatformDebugModule, GamePlatformDebug)

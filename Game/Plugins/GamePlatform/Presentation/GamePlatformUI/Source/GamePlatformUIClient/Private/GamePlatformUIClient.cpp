#include "Modules/ModuleManager.h"

class FGamePlatformUIClientModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FGamePlatformUIClientModule, GamePlatformUIClient)
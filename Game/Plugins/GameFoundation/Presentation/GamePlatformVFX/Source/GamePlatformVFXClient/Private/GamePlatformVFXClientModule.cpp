#include "Modules/ModuleManager.h"

class FGamePlatformVFXClientModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FGamePlatformVFXClientModule, GamePlatformVFXClient)

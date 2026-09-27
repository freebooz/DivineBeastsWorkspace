#include "Modules/ModuleManager.h"

class FGamePlatformVFXEditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override {}
    virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FGamePlatformVFXEditorModule, GamePlatformVFXEditor)

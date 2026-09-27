#include "Modules/ModuleManager.h"

#include "Features/IModularFeatures.h"
#include "Server/DivineBeastsArenaServerProjectExtension.h"
#include "Server/GamePlatformArenaServerProjectExtension.h"

class FDivineBeastsArenaServerModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        ProjectExtension = MakeUnique<FDivineBeastsArenaServerProjectExtension>();
        IModularFeatures::Get().RegisterModularFeature(
            IGamePlatformArenaServerProjectExtension::GetModularFeatureName(),
            ProjectExtension.Get());
    }

    virtual void ShutdownModule() override
    {
        if (ProjectExtension)
        {
            IModularFeatures::Get().UnregisterModularFeature(
                IGamePlatformArenaServerProjectExtension::GetModularFeatureName(),
                ProjectExtension.Get());
            ProjectExtension.Reset();
        }
    }

private:
    TUniquePtr<FDivineBeastsArenaServerProjectExtension> ProjectExtension;
};

IMPLEMENT_MODULE(FDivineBeastsArenaServerModule, DivineBeastsArenaServer)

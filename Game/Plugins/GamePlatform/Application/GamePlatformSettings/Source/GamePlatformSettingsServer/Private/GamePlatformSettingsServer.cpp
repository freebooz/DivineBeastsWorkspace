#include "Modules/ModuleManager.h"

#include "Features/IModularFeatures.h"
#include "Interfaces/IGamePlatformSettingsProvider.h"
#include "Server/GamePlatformSettingsServerPersistenceProvider.h"

class FGamePlatformSettingsServerModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        PersistenceProvider =
            MakeUnique<FGamePlatformSettingsServerPersistenceProvider>();
        IModularFeatures::Get().RegisterModularFeature(
            IGamePlatformSettingsPersistenceProvider::GetModularFeatureName(),
            PersistenceProvider.Get());
    }

    virtual void ShutdownModule() override
    {
        if (PersistenceProvider)
        {
            IModularFeatures::Get().UnregisterModularFeature(
                IGamePlatformSettingsPersistenceProvider::
                    GetModularFeatureName(),
                PersistenceProvider.Get());
            PersistenceProvider.Reset();
        }
    }

private:
    TUniquePtr<FGamePlatformSettingsServerPersistenceProvider>
        PersistenceProvider;
};

IMPLEMENT_MODULE(
    FGamePlatformSettingsServerModule,
    GamePlatformSettingsServer)

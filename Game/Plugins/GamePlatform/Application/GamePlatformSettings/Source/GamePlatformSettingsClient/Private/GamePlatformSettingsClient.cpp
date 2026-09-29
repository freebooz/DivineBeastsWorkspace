#include "Modules/ModuleManager.h"

#include "Features/IModularFeatures.h"
#include "Interfaces/IGamePlatformSettingsProvider.h"
#include "Persistence/GamePlatformSettingsClientPersistenceProvider.h"

class FGamePlatformSettingsClientModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        PersistenceProvider =
            MakeUnique<FGamePlatformSettingsClientPersistenceProvider>();
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
    /** 客户端唯一User Profile持久化适配器；不持有GameInstance/World。 */
    TUniquePtr<FGamePlatformSettingsClientPersistenceProvider>
        PersistenceProvider;
};

IMPLEMENT_MODULE(
    FGamePlatformSettingsClientModule,
    GamePlatformSettingsClient)

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
    /** 进程级无用户工厂；Runtime克隆GI独占适配器，不在此对象切换用户。 */
    TUniquePtr<FGamePlatformSettingsClientPersistenceProvider>
        PersistenceProvider;
};

IMPLEMENT_MODULE(
    FGamePlatformSettingsClientModule,
    GamePlatformSettingsClient)

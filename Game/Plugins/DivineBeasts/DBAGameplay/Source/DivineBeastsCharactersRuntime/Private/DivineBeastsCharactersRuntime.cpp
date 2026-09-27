#include "Modules/ModuleManager.h"

#include "Creation/DivineBeastsCharacterCreationProvider.h"
#include "Creation/DivineBeastsCharacterCreationProviderFactory.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "Features/IModularFeatures.h"

class FDivineBeastsCharactersRuntimeModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        CreationProvider = CreateDivineBeastsCharacterCreationProvider();
        if (CreationProvider.IsValid())
        {
            IModularFeatures::Get().RegisterModularFeature(
                IGamePlatformCharacterCreationProvider::GetModularFeatureName(),
                CreationProvider.Get());
        }
    }

    virtual void ShutdownModule() override
    {
        if (CreationProvider.IsValid())
        {
            IModularFeatures::Get().UnregisterModularFeature(
                IGamePlatformCharacterCreationProvider::GetModularFeatureName(),
                CreationProvider.Get());
            CreationProvider.Reset();
        }
    }

private:
    TUniquePtr<IDivineBeastsCharacterCreationProvider> CreationProvider;
};

IMPLEMENT_MODULE(
    FDivineBeastsCharactersRuntimeModule,
    DivineBeastsCharactersRuntime)

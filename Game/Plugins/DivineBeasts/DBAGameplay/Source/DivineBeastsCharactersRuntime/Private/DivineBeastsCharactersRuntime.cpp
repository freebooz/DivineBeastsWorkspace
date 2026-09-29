#include "Modules/ModuleManager.h"

#include "Creation/DivineBeastsCharacterCreationProvider.h"
#include "Creation/DivineBeastsCharacterCreationProviderFactory.h"
#include "Creation/GamePlatformCharacterCreationProvider.h"
#include "Features/IModularFeatures.h"
#include "Initialization/DivineBeastsCharacterSpawnInitializer.h"
#include "Initialization/GamePlatformCharacterInitializer.h"

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

        // 出生初始化器仅注册能力，不在模块启动阶段创建Pawn或访问世界；真正调用由平台统一Spawn Operation负责。
        SpawnInitializer = MakeUnique<FDivineBeastsCharacterSpawnInitializer>();
        IModularFeatures::Get().RegisterModularFeature(
            IGamePlatformCharacterInitializer::GetModularFeatureName(),
            SpawnInitializer.Get());
    }

    virtual void ShutdownModule() override
    {
        if (SpawnInitializer.IsValid())
        {
            IModularFeatures::Get().UnregisterModularFeature(
                IGamePlatformCharacterInitializer::GetModularFeatureName(),
                SpawnInitializer.Get());
            SpawnInitializer.Reset();
        }

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
    /** 项目层唯一角色出生初始化器；生命周期与模块一致。 */
    TUniquePtr<FDivineBeastsCharacterSpawnInitializer> SpawnInitializer;
};

IMPLEMENT_MODULE(
    FDivineBeastsCharactersRuntimeModule,
    DivineBeastsCharactersRuntime)

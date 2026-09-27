#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Server/GamePlatformHttpControlProvider.h"

class FGamePlatformServerModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        // 只注册传输适配机制；模块启动不读取凭据、不创建HTTP请求。
        ControlProvider = MakeUnique<FGamePlatformHttpControlProvider>();
        IModularFeatures::Get().RegisterModularFeature(
            IGamePlatformServerControlProvider::GetModularFeatureName(),
            ControlProvider.Get());
    }

    virtual void ShutdownModule() override
    {
        if (ControlProvider)
        {
            IModularFeatures::Get().UnregisterModularFeature(
                IGamePlatformServerControlProvider::GetModularFeatureName(),
                ControlProvider.Get());
            ControlProvider.Reset();
        }
    }

private:
    TUniquePtr<FGamePlatformHttpControlProvider> ControlProvider;
};

IMPLEMENT_MODULE(FGamePlatformServerModule, GamePlatformServer)

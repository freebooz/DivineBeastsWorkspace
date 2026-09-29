#include "Modules/ModuleManager.h"
#include "Features/IModularFeatures.h"
#include "Server/GamePlatformHttpControlProvider.h"
#include "Server/GamePlatformHttpAdmissionProvider.h"

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

        AdmissionProvider = MakeUnique<FGamePlatformHttpAdmissionProvider>();
        IModularFeatures::Get().RegisterModularFeature(
            IGamePlatformServerAdmissionProvider::GetModularFeatureName(),
            AdmissionProvider.Get());
    }

    virtual void ShutdownModule() override
    {
        if (AdmissionProvider)
        {
            IModularFeatures::Get().UnregisterModularFeature(
                IGamePlatformServerAdmissionProvider::GetModularFeatureName(),
                AdmissionProvider.Get());
            AdmissionProvider.Reset();
        }
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
    TUniquePtr<FGamePlatformHttpAdmissionProvider> AdmissionProvider;
};

IMPLEMENT_MODULE(FGamePlatformServerModule, GamePlatformServer)

// 平台服务器机制注册模块：持有控制/准入提供者与PostLogin订阅，关闭先解绑再释放。
// 仅权威World接入握手组件；启动阶段不登录、不读取凭据、不连接生产后端。
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "Server/GamePlatformHttpControlProvider.h"
#include "Server/GamePlatformHttpAdmissionProvider.h"
#include "Server/GamePlatformGameplayAdmissionHandler.h"
#include "Components/GamePlatformAdmissionHandshakeComponent.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"

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

        GameplayAdmissionHandler =
            MakeUnique<FGamePlatformServerGameplayAdmissionHandler>();
        IModularFeatures::Get().RegisterModularFeature(
            IGamePlatformGameplayAdmissionProofHandler::GetModularFeatureName(),
            GameplayAdmissionHandler.Get());

        // 使用引擎真实PostLogin事件为任意PlayerController动态挂载双端握手组件，
        // 不要求项目强制继承某个特定Controller基类。
        PostLoginHandle =
            FGameModeEvents::OnGameModePostLoginEvent().AddRaw(
                this,
                &FGamePlatformServerModule::HandlePostLogin);
    }

    virtual void ShutdownModule() override
    {
        if (PostLoginHandle.IsValid())
        {
            FGameModeEvents::OnGameModePostLoginEvent().Remove(
                PostLoginHandle);
            PostLoginHandle.Reset();
        }
        if (GameplayAdmissionHandler)
        {
            IModularFeatures::Get().UnregisterModularFeature(
                IGamePlatformGameplayAdmissionProofHandler::
                    GetModularFeatureName(),
                GameplayAdmissionHandler.Get());
            GameplayAdmissionHandler.Reset();
        }
        if (AdmissionProvider)
        {
            IModularFeatures::Get().UnregisterModularFeature(
                IGamePlatformServerAdmissionProvider::GetModularFeatureName(),
                AdmissionProvider.Get());
            // 注销后先清除在飞HTTP回调；不能以释放Provider代替请求生命周期关闭。
            AdmissionProvider->Shutdown();
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
    void HandlePostLogin(
        AGameModeBase* GameMode,
        APlayerController* NewPlayer)
    {
        if (!GameMode || !NewPlayer || !NewPlayer->HasAuthority() ||
            !NewPlayer->GetWorld() ||
            NewPlayer->GetWorld()->GetNetMode() == NM_Client)
        {
            return;
        }

        if (NewPlayer->FindComponentByClass<
                UGamePlatformAdmissionHandshakeComponent>())
        {
            return;
        }

        UGamePlatformAdmissionHandshakeComponent* Component =
            NewObject<UGamePlatformAdmissionHandshakeComponent>(
                NewPlayer,
                TEXT("GamePlatformAdmissionHandshake"),
                RF_Transient);
        if (!Component)
        {
            return;
        }

        NewPlayer->AddInstanceComponent(Component);
        Component->SetIsReplicated(true);
        Component->RegisterComponent();
    }

    TUniquePtr<FGamePlatformHttpControlProvider> ControlProvider;
    TUniquePtr<FGamePlatformHttpAdmissionProvider> AdmissionProvider;
    TUniquePtr<FGamePlatformServerGameplayAdmissionHandler>
        GameplayAdmissionHandler;
    FDelegateHandle PostLoginHandle;
};

IMPLEMENT_MODULE(FGamePlatformServerModule, GamePlatformServer)

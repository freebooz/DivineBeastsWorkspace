// 平台客户端SFX提供者适配：每个LocalPlayer注册/注销中立Presentation回调，转换请求后交给本World音频服务。
// 本桥不拥有播放实例或资源租约，不承担网络权威；失败的可选音效不改变Gameplay结果。
#include "Integration/Presentation/GamePlatformSFXPresentationBridgeSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h" // GetWorld传入UObject服务入口需完整UWorld继承类型，不能依赖PCH。
#include "GamePlatformPresentationClientSubsystem.h"
#include "Interfaces/IGamePlatformSFXService.h"

void UGamePlatformSFXPresentationBridgeSubsystem::Initialize(
    FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UGamePlatformPresentationClientSubsystem>();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PresentationSubsystem =
            LocalPlayer->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
    }

    if (IsValid(PresentationSubsystem))
    {
        ProviderRegistrationId = PresentationSubsystem->RegisterProvider(
            TEXT("SFX"),
            100,
            FGamePlatformPresentationProviderHandler::CreateUObject(
                this,
                &UGamePlatformSFXPresentationBridgeSubsystem::HandlePresentationRequest));
    }
}

void UGamePlatformSFXPresentationBridgeSubsystem::Deinitialize()
{
    if (IsValid(PresentationSubsystem) && ProviderRegistrationId.IsValid())
    {
        PresentationSubsystem->UnregisterProvider(ProviderRegistrationId);
    }
    ProviderRegistrationId.Invalidate();
    PresentationSubsystem = nullptr;
    Super::Deinitialize();
}

bool UGamePlatformSFXPresentationBridgeSubsystem::HandlePresentationRequest(
    const FGamePlatformPresentationRequest& Request)
{
    if (!Request.IsValid() || Request.ProviderChannel != TEXT("SFX"))
    {
        return false;
    }

    IGamePlatformSFXService* Service = IGamePlatformSFXService::Get(GetWorld());
    if (!Service)
    {
        return true;
    }

    FGamePlatformSFXRequest SFXRequest;
    SFXRequest.RequestId = Request.RequestId;
    switch (Request.PredictionState)
    {
    case EGamePlatformPresentationPredictionState::Predicted: SFXRequest.PredictionState=EGamePlatformSFXPredictionState::Predicted; break;
    case EGamePlatformPresentationPredictionState::Confirmed: SFXRequest.PredictionState=EGamePlatformSFXPredictionState::Confirmed; break;
    case EGamePlatformPresentationPredictionState::Corrected: SFXRequest.PredictionState=EGamePlatformSFXPredictionState::Corrected; break;
    case EGamePlatformPresentationPredictionState::Cancelled: SFXRequest.PredictionState=EGamePlatformSFXPredictionState::Cancelled; break;
    default: break;
    }
    SFXRequest.DefinitionId = Request.DefinitionId;
    SFXRequest.ContextId = Request.ContextId;
    SFXRequest.ContextTags = Request.ContextTags;
    SFXRequest.Location = Request.SourceLocation;

    Service->Play(SFXRequest);
    return true;
}

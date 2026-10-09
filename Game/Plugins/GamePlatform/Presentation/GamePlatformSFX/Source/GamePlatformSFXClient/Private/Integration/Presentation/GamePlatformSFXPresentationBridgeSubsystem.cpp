// 平台客户端音效桥接：本地玩家注册中立表现提供者，由当前世界音效服务执行。
// 不持有玩法权威；失活时撤销自己的注册，世界服务自行管理声音实例及数据租约。
#include "Integration/Presentation/GamePlatformSFXPresentationBridgeSubsystem.h"

#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
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

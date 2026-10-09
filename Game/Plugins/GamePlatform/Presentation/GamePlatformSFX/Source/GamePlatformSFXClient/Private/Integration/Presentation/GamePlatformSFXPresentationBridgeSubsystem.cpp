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

    if (Request.PredictionState == EGamePlatformPresentationPredictionState::Cancelled)
    {
        Service->StopByRequestId(Request.RequestId);
        return true;
    }

    // Corrected（预测纠正）不能沿用普通去重，否则旧预测声音会继续播放。
    // 先终止同RequestId旧实例，再使用纠正后的空间/Definition重新提交。
    if (Request.PredictionState == EGamePlatformPresentationPredictionState::Corrected)
    {
        Service->StopByRequestId(Request.RequestId, 0.0f);
    }

    FGamePlatformSFXRequest SFXRequest;
    SFXRequest.RequestId = Request.RequestId;
    SFXRequest.DefinitionId = Request.DefinitionId;
    SFXRequest.ContextId = Request.ContextId;
    SFXRequest.ContextTags = Request.ContextTags;
    SFXRequest.Location = Request.SourceLocation;

    Service->Play(SFXRequest);
    return true;
}

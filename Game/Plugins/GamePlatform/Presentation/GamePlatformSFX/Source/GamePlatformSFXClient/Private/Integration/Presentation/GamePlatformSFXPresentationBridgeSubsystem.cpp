#include "Integration/Presentation/GamePlatformSFXPresentationBridgeSubsystem.h"

#include "Engine/LocalPlayer.h"
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

    FGamePlatformSFXRequest SFXRequest;
    SFXRequest.RequestId = Request.RequestId;
    SFXRequest.DefinitionId = Request.DefinitionId;
    SFXRequest.ContextId = Request.ContextId;
    SFXRequest.ContextTags = Request.ContextTags;
    SFXRequest.Location = Request.SourceLocation;

    Service->Play(SFXRequest);
    return true;
}

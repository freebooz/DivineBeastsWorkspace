#include "Integration/Presentation/GamePlatformVFXPresentationBridgeSubsystem.h"
#include "Integration/Presentation/GamePlatformVFXPresentationProvider.h"
#include "GamePlatformPresentationClientSubsystem.h"
#include "Engine/LocalPlayer.h"

void UGamePlatformVFXPresentationBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    Collection.InitializeDependency<UGamePlatformPresentationClientSubsystem>();

    if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
    {
        PresentationSubsystem = LocalPlayer->GetSubsystem<UGamePlatformPresentationClientSubsystem>();
    }

    if (IsValid(PresentationSubsystem))
    {
        ProviderRegistrationId = PresentationSubsystem->RegisterProvider(
            TEXT("VFX"),
            100,
            FGamePlatformPresentationProviderHandler::CreateUObject(
                this,
                &UGamePlatformVFXPresentationBridgeSubsystem::HandlePresentationRequest));
    }
}

void UGamePlatformVFXPresentationBridgeSubsystem::Deinitialize()
{
    if (IsValid(PresentationSubsystem) && ProviderRegistrationId.IsValid())
    {
        PresentationSubsystem->UnregisterProvider(ProviderRegistrationId);
    }
    ProviderRegistrationId.Invalidate();
    PresentationSubsystem = nullptr;
    Super::Deinitialize();
}

bool UGamePlatformVFXPresentationBridgeSubsystem::HandlePresentationRequest(
    const FGamePlatformPresentationRequest& Request)
{
    FGamePlatformVFXPresentationProvider Provider(GetWorld());
    return Provider.Handle(Request);
}

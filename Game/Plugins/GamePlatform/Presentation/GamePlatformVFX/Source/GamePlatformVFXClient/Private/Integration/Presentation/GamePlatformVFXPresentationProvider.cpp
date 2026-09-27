#include "Integration/Presentation/GamePlatformVFXPresentationProvider.h"
#include "Interfaces/GamePlatformVFXService.h"
#include "Types/GamePlatformVFXRequest.h"
#include "Engine/World.h"
#include "HAL/PlatformProperties.h"

bool FGamePlatformVFXPresentationProvider::Handle(
    const FGamePlatformPresentationRequest& Request) const
{
    if (!Request.IsValid() || Request.ProviderChannel != TEXT("VFX"))
    {
        return false;
    }

    IGamePlatformVFXService* Service = IGamePlatformVFXService::Get(World.Get());
    if (!Service)
    {
        return false;
    }

    FGamePlatformVFXRequest VFXRequest;
    VFXRequest.RequestId = Request.RequestId;
    VFXRequest.ActivationId = Request.RequestId;
    VFXRequest.PredictionKey = Request.RequestGeneration;
    VFXRequest.SemanticTag = Request.SemanticTag;
    VFXRequest.ContextId = Request.ContextId;
    VFXRequest.ContextTags = Request.ContextTags;
    VFXRequest.DefinitionId = Request.DefinitionId;
    VFXRequest.PlatformId = FName(FPlatformProperties::IniPlatformName());
    VFXRequest.SpawnContext.Location = Request.SourceLocation;

    switch (Request.Priority)
    {
    case EGamePlatformPresentationPriority::Critical:
        VFXRequest.Importance = EGamePlatformVFXImportance::Critical;
        break;
    case EGamePlatformPresentationPriority::High:
        VFXRequest.Importance = EGamePlatformVFXImportance::Combat;
        break;
    case EGamePlatformPresentationPriority::Normal:
        VFXRequest.Importance = EGamePlatformVFXImportance::Status;
        break;
    case EGamePlatformPresentationPriority::Low:
    default:
        VFXRequest.Importance = EGamePlatformVFXImportance::Ambient;
        break;
    }

    switch (Request.PredictionState)
    {
    case EGamePlatformPresentationPredictionState::Predicted:
        VFXRequest.PredictionState = EGamePlatformVFXPredictionState::Predicted;
        break;
    case EGamePlatformPresentationPredictionState::Cancelled:
        VFXRequest.PredictionState = EGamePlatformVFXPredictionState::Cancelled;
        break;
    case EGamePlatformPresentationPredictionState::Confirmed:
    case EGamePlatformPresentationPredictionState::Corrected:
    default:
        VFXRequest.PredictionState = EGamePlatformVFXPredictionState::Confirmed;
        break;
    }

    Service->Play(VFXRequest);
    return true;
}

#pragma once

#include "GamePlatformPresentationTypes.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformSFXPresentationBridgeSubsystem.generated.h"

class UGamePlatformPresentationClientSubsystem;

/**
 * UGamePlatformSFXPresentationBridgeSubsystem（SFX表现桥）。
 * Gameplay只提交平台中立Presentation请求，本桥仅转换ProviderChannel=SFX的请求。
 */
UCLASS()
class UGamePlatformSFXPresentationBridgeSubsystem final : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    bool HandlePresentationRequest(const FGamePlatformPresentationRequest& Request);

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformPresentationClientSubsystem> PresentationSubsystem = nullptr;

    FGuid ProviderRegistrationId;
};

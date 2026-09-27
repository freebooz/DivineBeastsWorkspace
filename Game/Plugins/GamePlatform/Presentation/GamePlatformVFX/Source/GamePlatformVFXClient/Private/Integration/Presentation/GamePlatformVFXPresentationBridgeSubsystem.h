#pragma once

#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformPresentationTypes.h"
#include "GamePlatformVFXPresentationBridgeSubsystem.generated.h"

class UGamePlatformPresentationClientSubsystem;

/** LocalPlayer级VFX表现桥；Gameplay仍只依赖中立Presentation协议。 */
UCLASS()
class UGamePlatformVFXPresentationBridgeSubsystem final : public ULocalPlayerSubsystem
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

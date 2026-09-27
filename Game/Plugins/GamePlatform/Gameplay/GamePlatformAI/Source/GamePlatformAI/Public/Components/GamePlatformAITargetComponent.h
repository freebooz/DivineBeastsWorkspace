#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interfaces/GamePlatformAITargetEligibilityProvider.h"
#include "GamePlatformAITargetComponent.generated.h"

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMAI_API UGamePlatformAITargetComponent final
    : public UActorComponent
    , public IGamePlatformAITargetEligibilityProvider
{
    GENERATED_BODY()

public:
    UGamePlatformAITargetComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual bool IsEligibleAsAITarget(const AActor* RequestingAI) const override
    {
        return bAITargetEnabled;
    }

    virtual FGuid GetAITargetEntityId() const override
    {
        return TargetEntityId;
    }

    virtual int32 GetAITargetGeneration() const override
    {
        return TargetGeneration;
    }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI")
    void SetAITargetEnabled(bool bEnabled);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI")
    void AdvanceTargetGeneration();

private:
    UPROPERTY(Replicated)
    FGuid TargetEntityId;

    UPROPERTY(Replicated)
    int32 TargetGeneration = 1;

    UPROPERTY(Replicated)
    bool bAITargetEnabled = true;
};

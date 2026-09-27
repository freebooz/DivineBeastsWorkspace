#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GamePlatformAITypes.h"
#include "GamePlatformAIStateComponent.generated.h"

class UGamePlatformAIDefinition;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformAIStateChanged,
    const FGamePlatformAIStateSnapshot&, Snapshot);

UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMAI_API UGamePlatformAIStateComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformAIStateComponent();

    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="AI")
    const FGamePlatformAIStateSnapshot& GetSnapshot() const
    {
        return Snapshot;
    }

    UFUNCTION(BlueprintPure, Category="AI")
    TSoftObjectPtr<UGamePlatformAIDefinition> GetDefinitionAsset() const
    {
        return DefinitionAsset;
    }

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI")
    void SetDefinitionAsset(
        TSoftObjectPtr<UGamePlatformAIDefinition> InDefinition);

    void InitializeServerState(
        FName DefinitionId,
        int32 Generation);

    void SetServerPublicState(EGamePlatformAIPublicState NewState);
    void SetServerTargetEntityId(const FGuid& TargetEntityId);
    void SetServerIntentTags(
        const FGameplayTag& MovementIntent,
        const FGameplayTag& CombatIntent);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="AI")
    void AdvanceServerGeneration();

    UPROPERTY(BlueprintAssignable, Category="AI")
    FGamePlatformAIStateChanged OnStateChanged;

private:
    UPROPERTY(EditAnywhere, Category="AI")
    TSoftObjectPtr<UGamePlatformAIDefinition> DefinitionAsset;

    UPROPERTY(ReplicatedUsing=OnRep_Snapshot)
    FGamePlatformAIStateSnapshot Snapshot;

    UFUNCTION()
    void OnRep_Snapshot();

    void TouchRevision();
};

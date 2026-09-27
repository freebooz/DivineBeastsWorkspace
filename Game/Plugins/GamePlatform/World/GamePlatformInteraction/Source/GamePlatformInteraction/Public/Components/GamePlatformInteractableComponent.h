#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Types/GamePlatformInteractionOption.h"
#include "Types/GamePlatformInteractionResult.h"
#include "GamePlatformInteractableComponent.generated.h"

class UGamePlatformInteractorComponent;
struct FGamePlatformInteractionSession;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGamePlatformInteractableStateChanged);

/**
 * 统一可交互目标状态组件。
 * Target身份、Revision、Availability和世界测试状态均由服务器权威并复制。
 */
UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMINTERACTION_API UGamePlatformInteractableComponent final
    : public USceneComponent
{
    GENERATED_BODY()

public:
    UGamePlatformInteractableComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category="Interaction")
    const FGuid& GetTargetInstanceId() const { return TargetInstanceId; }

    UFUNCTION(BlueprintPure, Category="Interaction")
    int32 GetTargetGeneration() const { return TargetGeneration; }

    UFUNCTION(BlueprintPure, Category="Interaction")
    int32 GetTargetRevision() const { return TargetRevision; }

    UFUNCTION(BlueprintPure, Category="Interaction")
    bool IsInteractionEnabled() const { return bEnabled && !bConsumed; }

    UFUNCTION(BlueprintPure, Category="Interaction")
    bool IsConsumed() const { return bConsumed; }

    UFUNCTION(BlueprintPure, Category="Interaction")
    bool GetToggleState() const { return bToggleState; }

    UFUNCTION(BlueprintPure, Category="Interaction")
    int32 GetRemainingCharges() const { return RemainingCharges; }

    UFUNCTION(BlueprintPure, Category="Interaction")
    int32 GetOccupancyCount() const { return OccupancyCount; }

    UFUNCTION(BlueprintPure, Category="Interaction")
    FVector GetInteractionPoint() const { return GetComponentLocation(); }

    const TArray<FGamePlatformInteractionOption>& GetOptions() const
    {
        return Options;
    }

    const FGamePlatformInteractionOption* FindOption(FName OptionId) const;

    bool IsOptionAvailable(
        const FGamePlatformInteractionOption& Option) const;

    bool CanContinueSession(
        const FGuid& SessionId,
        const FGamePlatformInteractionOption& Option) const;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Interaction")
    bool SetOptions(const TArray<FGamePlatformInteractionOption>& InOptions);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Interaction")
    bool SetInteractionEnabled(bool bInEnabled);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Interaction")
    bool SetRemainingCharges(int32 InRemainingCharges);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Interaction")
    void AdvanceTargetGeneration();

    bool TryAcquireSession(
        const FGuid& SessionId,
        UGamePlatformInteractorComponent* Interactor,
        const FGamePlatformInteractionOption& Option,
        EGamePlatformInteractionError& OutError);

    void ReleaseSession(const FGuid& SessionId);

    bool CommitSession(
        const FGamePlatformInteractionSession& Session,
        const FGamePlatformInteractionOption& Option,
        FGamePlatformInteractionResult& OutResult);

    /**
     * External（外部结果）提交使用的服务器权威保留接口。
     * Reservation期间目标对新的Interaction不可用，但不会提前标记Consumed。
     */
    bool BeginExternalOutcomeReservation(const FGuid& ReservationId);
    bool FinalizeExternalConsume(const FGuid& ReservationId);
    bool CancelExternalOutcomeReservation(const FGuid& ReservationId);

    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FGamePlatformInteractableStateChanged OnStateChanged;

private:
    UPROPERTY(ReplicatedUsing=OnRep_State)
    FGuid TargetInstanceId;

    UPROPERTY(ReplicatedUsing=OnRep_State)
    int32 TargetGeneration = 1;

    UPROPERTY(ReplicatedUsing=OnRep_State)
    int32 TargetRevision = 1;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_State, Category="Interaction")
    TArray<FGamePlatformInteractionOption> Options;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_State, Category="Interaction")
    bool bEnabled = true;

    UPROPERTY(ReplicatedUsing=OnRep_State)
    bool bConsumed = false;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_State, Category="Interaction|Development")
    bool bToggleState = false;

    UPROPERTY(EditAnywhere, ReplicatedUsing=OnRep_State, Category="Interaction|Development", meta=(ClampMin="0"))
    int32 RemainingCharges = 0;

    UPROPERTY(ReplicatedUsing=OnRep_State)
    int32 OccupancyCount = 0;

    TMap<FGuid, TWeakObjectPtr<UGamePlatformInteractorComponent>> ActiveSessions;
    TSet<FGuid> CommittedSessions;
    TArray<FGuid> CommittedSessionOrder;
    FGuid ExternalOutcomeReservationId;

    UFUNCTION()
    void OnRep_State();

    void CancelActiveSessions(
        EGamePlatformInteractionCancelReason Reason);
    void BumpRevision();
    bool ValidateOptions(
        const TArray<FGamePlatformInteractionOption>& InOptions) const;
};

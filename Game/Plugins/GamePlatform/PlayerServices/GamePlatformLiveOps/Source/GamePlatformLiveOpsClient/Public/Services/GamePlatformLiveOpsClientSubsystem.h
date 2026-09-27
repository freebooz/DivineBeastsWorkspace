#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Time/GamePlatformLiveOpsServerTimeEstimator.h"
#include "Types/GamePlatformLiveOpsTypes.h"
#include "GamePlatformLiveOpsClientSubsystem.generated.h"

class IGamePlatformLiveOpsClientTransport;

DECLARE_MULTICAST_DELEGATE(FGamePlatformLiveOpsChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformLiveOpsClaimChanged,
    FGamePlatformLiveOpsClaimResult);

UCLASS()
class GAMEPLATFORMLIVEOPSCLIENT_API UGamePlatformLiveOpsClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(
        FSubsystemCollectionBase& Collection) override;

    virtual void Deinitialize() override;

    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformLiveOpsClientTransport, ESPMode::ThreadSafe>
            InTransport);

    void ResetAccount();

    UFUNCTION(BlueprintCallable, Category="LiveOps")
    bool RefreshAll();

    UFUNCTION(BlueprintCallable, Category="LiveOps")
    bool RefreshCatalog();

    UFUNCTION(BlueprintCallable, Category="LiveOps")
    bool RefreshPlayerState();

    bool ClaimSignIn(
        FName CampaignId,
        const FGuid& ClaimOperationId);

    bool ReconcileClaim(
        const FGuid& ClaimOperationId);

    UFUNCTION(BlueprintPure, Category="LiveOps")
    EGamePlatformLiveOpsClientState GetState() const
    {
        return State;
    }

    UFUNCTION(BlueprintPure, Category="LiveOps")
    FDateTime GetEstimatedServerNowUtc() const
    {
        return ServerTimeEstimator.EstimatedServerNowUtc();
    }

    UFUNCTION(BlueprintPure, Category="LiveOps")
    TArray<FName> GetActiveSeasonIds() const;

    UFUNCTION(BlueprintPure, Category="LiveOps")
    TArray<FGamePlatformLiveOpsEventViewModel> GetEventViewModels() const;

    UFUNCTION(BlueprintPure, Category="LiveOps")
    TArray<FGamePlatformLiveOpsSignInViewModel> GetSignInViewModels() const;

    UFUNCTION(BlueprintPure, Category="LiveOps")
    int64 GetCatalogRevision() const
    {
        return Catalog.CatalogRevision;
    }

    UFUNCTION(BlueprintPure, Category="LiveOps")
    int64 GetPlayerStateRevision() const
    {
        return PlayerState.PlayerStateRevision;
    }

    FGamePlatformLiveOpsChanged OnCatalogChanged;
    FGamePlatformLiveOpsChanged OnPlayerStateChanged;
    FGamePlatformLiveOpsClaimChanged OnClaimChanged;

private:
    FString CurrentAccountKey;
    uint64 AccountGeneration = 0;

    EGamePlatformLiveOpsClientState State =
        EGamePlatformLiveOpsClientState::Uninitialized;

    EGamePlatformLiveOpsError LastError =
        EGamePlatformLiveOpsError::None;

    FGamePlatformLiveOpsCatalogSnapshot Catalog;
    FGamePlatformLiveOpsPlayerState PlayerState;
    FGamePlatformLiveOpsClaimResult LastClaim;
    FGamePlatformLiveOpsServerTimeEstimator ServerTimeEstimator;

    TSharedPtr<IGamePlatformLiveOpsClientTransport, ESPMode::ThreadSafe>
        Transport;

    FDelegateHandle ForegroundHandle;
    FTSTicker::FDelegateHandle BoundaryTickerHandle;
    FDateTime LastBoundaryCheckUtc;

    bool bCatalogRequestInFlight = false;
    bool bPlayerStateRequestInFlight = false;
    bool bClaimRequestInFlight = false;
    bool bClaimReconcileInFlight = false;
    FGuid ActiveClaimOperationId;

    void HandleEnteredForeground();
    bool TickBoundaryRefresh(float DeltaSeconds);
    bool HasCrossedCatalogBoundary(
        const FDateTime& PreviousUtc,
        const FDateTime& CurrentUtc) const;

    void HandleCatalogCompleted(
        uint64 ExpectedGeneration,
        FGamePlatformLiveOpsCatalogSnapshot NewCatalog,
        EGamePlatformLiveOpsError Error);

    void HandlePlayerStateCompleted(
        uint64 ExpectedGeneration,
        FGamePlatformLiveOpsPlayerState NewState,
        EGamePlatformLiveOpsError Error);

    void HandleClaimCompleted(
        uint64 ExpectedGeneration,
        FGamePlatformLiveOpsClaimResult Result,
        EGamePlatformLiveOpsError Error);

    void RefreshAggregateState();
};

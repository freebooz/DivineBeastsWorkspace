#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/GamePlatformCommerceUITypes.h"
#include "GamePlatformCommerceClientSubsystem.generated.h"

class IGamePlatformCommerceClientTransport;
class UGamePlatformCommerceViewModel;

DECLARE_MULTICAST_DELEGATE(FGamePlatformCommerceStateChanged);
DECLARE_MULTICAST_DELEGATE_OneParam(
    FGamePlatformCommerceOrderChanged,
    FGamePlatformCommerceOrderStatusView);

UCLASS()
class GAMEPLATFORMCOMMERCEUICLIENT_API UGamePlatformCommerceClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformCommerceClientTransport, ESPMode::ThreadSafe>
            InTransport);

    void ResetAccount();

    UFUNCTION(BlueprintCallable, Category="Commerce")
    bool RefreshCatalog();

    bool CreatePurchaseIntent(
        FName OfferId,
        int32 Quantity,
        const FGuid& RequestId);

    bool BeginPurchase(const FString& PurchaseIntentId);

    bool SubmitReceipt(
        const FString& OrderId,
        const FString& Receipt);

    bool RefreshOrder(const FString& OrderId);

    bool ReconcileOrder(const FString& OrderId);

    UFUNCTION(BlueprintPure, Category="Commerce")
    EGamePlatformCommerceClientState GetState() const
    {
        return State;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    int64 GetCatalogRevision() const
    {
        return Catalog.CatalogRevision;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    UGamePlatformCommerceViewModel* GetCommerceViewModel() const
    {
        return ViewModel;
    }

    FGamePlatformCommerceStateChanged OnCommerceStateChanged;
    FGamePlatformCommerceOrderChanged OnOrderChanged;

private:
    FString CurrentAccountKey;
    uint64 AccountGeneration = 0;

    EGamePlatformCommerceClientState State =
        EGamePlatformCommerceClientState::Idle;

    EGamePlatformCommerceError LastError =
        EGamePlatformCommerceError::None;

    FGamePlatformCommerceCatalogSnapshot Catalog;
    FGamePlatformCommercePurchaseIntentView CurrentIntent;
    FGamePlatformCommerceOrderStatusView CurrentOrder;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformCommerceViewModel> ViewModel = nullptr;

    TSharedPtr<IGamePlatformCommerceClientTransport, ESPMode::ThreadSafe>
        Transport;

    bool bCatalogRequestInFlight = false;
    bool bIntentRequestInFlight = false;
    bool bOrderRequestInFlight = false;

    void HandleCatalog(
        uint64 ExpectedGeneration,
        FGamePlatformCommerceCatalogSnapshot Snapshot,
        EGamePlatformCommerceError Error);

    void HandleIntent(
        uint64 ExpectedGeneration,
        FGamePlatformCommercePurchaseIntentView Intent,
        EGamePlatformCommerceError Error);

    void HandleOrder(
        uint64 ExpectedGeneration,
        FGamePlatformCommerceOrderStatusView Order,
        EGamePlatformCommerceError Error);

    void SetState(EGamePlatformCommerceClientState NewState);
    void ApplyOrderState(
        const FGamePlatformCommerceOrderStatusView& Order);
};

#pragma once

#include "CoreMinimal.h"
#include "ViewModels/GamePlatformViewModelBase.h"
#include "Types/GamePlatformCommerceUITypes.h"
#include "GamePlatformCommerceViewModel.generated.h"

UCLASS(BlueprintType)
class GAMEPLATFORMCOMMERCEUICLIENT_API UGamePlatformCommerceViewModel final
    : public UGamePlatformViewModelBase
{
    GENERATED_BODY()

public:
    void ApplyCatalog(
        const FGamePlatformCommerceCatalogSnapshot& InCatalog);

    void ApplyIntent(
        const FGamePlatformCommercePurchaseIntentView& InIntent);

    void ApplyOrder(
        const FGamePlatformCommerceOrderStatusView& InOrder);

    void ClearPlayerState();

    /** C++/Slate高频读取使用，避免Blueprint值返回造成的数组复制。 */
    const TArray<FGamePlatformCommerceProductCardView>& GetProductsView() const
    {
        return Catalog.Products;
    }

    const TArray<FGamePlatformCommerceOfferView>& GetOffersView() const
    {
        return Catalog.Offers;
    }

    const FGamePlatformCommerceCatalogSnapshot& GetCatalogView() const
    {
        return Catalog;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    TArray<FGamePlatformCommerceProductCardView> GetProducts() const
    {
        return Catalog.Products;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    TArray<FGamePlatformCommerceOfferView> GetOffers() const
    {
        return Catalog.Offers;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    FGamePlatformCommercePurchaseIntentView GetCurrentIntent() const
    {
        return CurrentIntent;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    FGamePlatformCommerceOrderStatusView GetCurrentOrder() const
    {
        return CurrentOrder;
    }

private:
    UPROPERTY(Transient)
    FGamePlatformCommerceCatalogSnapshot Catalog;

    UPROPERTY(Transient)
    FGamePlatformCommercePurchaseIntentView CurrentIntent;

    UPROPERTY(Transient)
    FGamePlatformCommerceOrderStatusView CurrentOrder;
};

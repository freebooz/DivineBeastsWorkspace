#include "ViewModels/GamePlatformCommerceViewModel.h"

void UGamePlatformCommerceViewModel::ApplyCatalog(
    const FGamePlatformCommerceCatalogSnapshot& InCatalog)
{
    Catalog = InCatalog;
    MarkStateChanged();
}

void UGamePlatformCommerceViewModel::ApplyIntent(
    const FGamePlatformCommercePurchaseIntentView& InIntent)
{
    CurrentIntent = InIntent;
    MarkStateChanged();
}

void UGamePlatformCommerceViewModel::ApplyOrder(
    const FGamePlatformCommerceOrderStatusView& InOrder)
{
    CurrentOrder = InOrder;
    MarkStateChanged();
}

void UGamePlatformCommerceViewModel::ClearPlayerState()
{
    CurrentIntent = {};
    CurrentOrder = {};
    MarkStateChanged();
}

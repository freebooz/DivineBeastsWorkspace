#pragma once

#include "CoreMinimal.h"
#include "Screens/GamePlatformUIScreen.h"
#include "GamePlatformCommerceScreens.generated.h"

/**
 * C++ reusable template for WBP_CommerceCatalog（商城目录蓝图模板）.
 * Real .uasset must be created in Unreal Editor（虚幻编辑器）.
 */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMCOMMERCEUICLIENT_API UGamePlatformCommerceCatalogScreen
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()
};

/** WBP_CommerceProductDetail（商品详情蓝图模板）C++ base. */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMCOMMERCEUICLIENT_API UGamePlatformCommerceProductDetailScreen
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()
};

/** WBP_CommercePurchaseConfirm（购买确认蓝图模板）C++ base. */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMCOMMERCEUICLIENT_API UGamePlatformCommercePurchaseConfirmScreen
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()
};

/** WBP_CommerceOrderStatus（订单状态蓝图模板）C++ base. */
UCLASS(Abstract, Blueprintable)
class GAMEPLATFORMCOMMERCEUICLIENT_API UGamePlatformCommerceOrderStatusScreen
    : public UGamePlatformUIScreen
{
    GENERATED_BODY()
};

// 本地玩家商店只读投影；金额为明确币种最小单位整数，业务真源/付款验证/发奖均在后端；账号清空撤销短期状态。
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
    /** 游戏线程接纳业务服务验证后的目录值副本并通知视图；ViewModel不请求HTTP或拥有价格权威。 */
    void ApplyCatalog(
        const FGamePlatformCommerceCatalogSnapshot& InCatalog);

    /** 游戏线程保存本次后端意向投影并通知，参数不得包含账号认证密码/令牌。 */
    void ApplyIntent(
        const FGamePlatformCommercePurchaseIntentView& InIntent);

    /** 游戏线程保存后端订单投影并派生显示标志；不调用支付或发奖。 */
    void ApplyOrder(
        const FGamePlatformCommerceOrderStatusView& InOrder);

    /** 游戏线程清空意向/订单/Provider短期材料并通知；公开目录可保留，不能代替真实注销/退款。 */
    void ClearPlayerState();

    /** 游戏线程借用产品只读数组，下一ApplyCatalog/退出前有效，避免重复复制。 */
    const TArray<FGamePlatformCommerceProductCardView>& GetProductsView() const
    {
        return Catalog.Products;
    }

    /** 游戏线程借用当前目录/方案只读投影；下一ApplyCatalog/退出后引用失效。 */
    const TArray<FGamePlatformCommerceOfferView>& GetOffersView() const
    {
        return Catalog.Offers;
    }

    /** 游戏线程借用当前目录/方案只读投影；下一ApplyCatalog/退出后引用失效。 */
    const FGamePlatformCommerceCatalogSnapshot& GetCatalogView() const
    {
        return Catalog;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    /** 游戏线程返回当前只读投影值副本；空值表示未加载/无本次操作，不构成交易成功。 */
    TArray<FGamePlatformCommerceProductCardView> GetProducts() const
    {
        return Catalog.Products;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    /** 游戏线程返回当前只读投影值副本；空值表示未加载/无本次操作，不构成交易成功。 */
    TArray<FGamePlatformCommerceOfferView> GetOffers() const
    {
        return Catalog.Offers;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    /** 游戏线程返回当前只读投影值副本；空值表示未加载/无本次操作，不构成交易成功。 */
    FGamePlatformCommercePurchaseIntentView GetCurrentIntent() const
    {
        return CurrentIntent;
    }

    UFUNCTION(BlueprintPure, Category="Commerce")
    /** 游戏线程返回当前只读投影值副本；空值表示未加载/无本次操作，不构成交易成功。 */
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

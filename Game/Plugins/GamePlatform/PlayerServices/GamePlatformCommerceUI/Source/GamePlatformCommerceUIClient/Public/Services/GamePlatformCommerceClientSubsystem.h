// 本地玩家商店只读投影；金额为明确币种最小单位整数，业务真源/付款验证/发奖均在后端；账号清空撤销短期状态。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformOnlineClientSubsystem.h"
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

    /** 游戏线程：非空账号/Transport，正式跟随Online；true只表示目录受理，交易必须单独等待后端终态。 */
    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformCommerceClientTransport, ESPMode::ThreadSafe>
            InTransport);

    /** 游戏线程取消本Port请求，清空意向/订单/Provider短期材料并通知；全局目录可保留，不注销Online或退款。 */
    void ResetAccount();

    UFUNCTION(BlueprintCallable, Category="Commerce")
    /** 游戏线程读取目录；忙/未认证返回false，成功受理通过状态与ViewModel事件通知结果。 */
    bool RefreshCatalog();

    /** 游戏线程：OfferId非None、Quantity正整数、RequestId有效且同一次操作保持不变；true受理，不扣款/发奖。 */
    bool CreatePurchaseIntent(
        FName OfferId,
        int32 Quantity,
        const FGuid& RequestId);

    /** 游戏线程：非空未过期购买意向身份；true仅表示订单命令受理，后端验证价格/资格/幂等。 */
    bool BeginPurchase(const FString& PurchaseIntentId);

    /** 游戏线程：非空OrderId和支付凭据；凭据不日志/配置，true仅受理验证，不能将SDK成功直接当成交。 */
    bool SubmitReceipt(
        const FString& OrderId,
        const FString& Receipt);

    /** 游戏线程查询非空原OrderId；true受理，不生成新订单、不重复付款，错误通过状态交付。 */
    bool RefreshOrder(const FString& OrderId);

    /** 游戏线程按原OrderId对账未知结果；true受理，不重发扣款/奖励，不确定结果保持Reconciling。 */
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
    /** 所属GI的Online仅弱引用；账号/请求与委托均由本地玩家作用域退出清理。 */
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> OnlineSubsystem;
    FDelegateHandle AuthStateChangedHandle;
    void BindOnlineAuthentication();
    void UnbindOnlineAuthentication();
    void HandleAuthStateChanged(const FGamePlatformAuthSnapshot& AuthSnapshot);
    /** Reset事件可再次请求Reset；正在清空时幂等忽略，禁止在同广播栈重新配置账号。 */
    bool bResettingAccount = false;
    /** 永久关闭当前实例作用域；仅Initialize可开启新代次，广播/Cancel重入不能复活服务。 */
    bool bDeinitializing = false;
    uint64 InstanceGeneration = 0;
    FString CurrentAccountKey;
    uint64 AccountGeneration = 0;
    /** 三种用途独立请求代次；上一轮同账号迟到完成也不能更新下一轮操作。 */
    uint64 CatalogRequestGeneration = 0;
    uint64 IntentRequestGeneration = 0;
    uint64 OrderRequestGeneration = 0;

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
        uint64 ExpectedRequestGeneration,
        FGamePlatformCommerceCatalogSnapshot Snapshot,
        EGamePlatformCommerceError Error);

    void HandleIntent(
        uint64 ExpectedGeneration,
        uint64 ExpectedRequestGeneration,
        FGamePlatformCommercePurchaseIntentView Intent,
        EGamePlatformCommerceError Error);

    void HandleOrder(
        uint64 ExpectedGeneration,
        uint64 ExpectedRequestGeneration,
        FGamePlatformCommerceOrderStatusView Order,
        EGamePlatformCommerceError Error);

    void SetState(EGamePlatformCommerceClientState NewState);
    void ApplyOrderState(
        const FGamePlatformCommerceOrderStatusView& Order);
};

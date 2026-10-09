#pragma once

// 平台本地玩家运营投影：服务器UTC派生活动/签到可见性，客户端不授奖。
// 游戏线程命令与完成；账号代次隔离旧响应，视图代次事件覆盖同Revision时间边界。

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformOnlineClientSubsystem.h"
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

    /** 已认证组合根设置账号与传输；拒绝空键/空Port，true仅表示目录或玩家读取受理，不授奖。 */
    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformLiveOpsClientTransport, ESPMode::ThreadSafe>
            InTransport);

    /** 取消本账号请求并清空玩家/领奖投影；公开运营目录可保留；事件通知清空且旧完成不能污染。 */
    void ResetAccount();

    UFUNCTION(BlueprintCallable, Category="LiveOps")
    /** 尝试刷新目录及玩家状态；任一读取受理即返回true，分别通过事件报告终态。 */
    bool RefreshAll();

    UFUNCTION(BlueprintCallable, Category="LiveOps")
    /** 目录读取忙或未认证配置返回false；相同服务器Revision的时间采样仍通知派生视图。 */
    bool RefreshCatalog();

    UFUNCTION(BlueprintCallable, Category="LiveOps")
    /** 刷新签到权威周期/领取状态；客户端不自行滚动PeriodKey，完成或错误通过事件报告。 */
    bool RefreshPlayerState();

    /** 提交签到命令；有效CampaignId和全局唯一操作Id必填；true只表示受理，超时按后端操作Id对账。 */
    bool ClaimSignIn(
        FName CampaignId,
        const FGuid& ClaimOperationId);

    /** 按原操作Id查询权威领奖结果；查询不重复发奖，忙或无账号拒绝受理。 */
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

    /** 状态/错误/清空/时间派生变化事件；消费方读取只读视图，无需业务Tick。 */
    FGamePlatformLiveOpsChanged OnViewChanged;
    /** 本地派生视图代次，时间边界/错误/清空均推进，与服务端目录/玩家Revision分离。 */
    uint64 GetViewGeneration() const { return ViewGeneration; }
    /** 最近领域终态错误；网络失败仍保留只读缓存，不能将缓存当授权或奖品到账证明。 */
    EGamePlatformLiveOpsError GetLastError() const { return LastError; }
    /** 目录Revision不变的刷新及开始/结束时间边界亦触发。 */
    FGamePlatformLiveOpsChanged OnCatalogChanged;
    /** 玩家签到投影、清空及活动边界通知；消费者重新读取GetSignInViewModels。 */
    FGamePlatformLiveOpsChanged OnPlayerStateChanged;
    /** 后端操作完成投影通知；随后重新读取玩家状态，不直接修改本地已领取标志。 */
    FGamePlatformLiveOpsClaimChanged OnClaimChanged;

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
    uint64 ViewGeneration = 0;
    /** 完整状态提交后通知指定派生视图；监听者重置账号时停止本轮后续派发。 */
    void PublishDerivedViewChanged(bool bCatalog, bool bPlayer);

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

    /** 只在真实Initialize后拥有核心Ticker；裸NewObject测试对象不注册全局运行回调。 */
    bool bLifecycleInitialized = false;
    FDelegateHandle ForegroundHandle;
    FTSTicker::FDelegateHandle BoundaryTickerHandle;
    FDateTime LastBoundaryCheckUtc;

    /** 每类请求独立终态代次；重复/迟到回调不得消费下一次刷新或领取资格。 */
    uint64 CatalogRequestGeneration = 0;
    uint64 PlayerStateRequestGeneration = 0;
    uint64 ClaimRequestGeneration = 0;
    uint64 ReconcileRequestGeneration = 0;
    bool bCatalogRequestInFlight = false;
    bool bPlayerStateRequestInFlight = false;
    bool bClaimRequestInFlight = false;
    bool bClaimReconcileInFlight = false;
    FGuid ActiveClaimOperationId;

    /** 仅有认证账号时安装低频UTC边界Ticker；账号清空即撤销，不以Tick弥补业务事件。 */
    void UpdateBoundaryTicker();
    void HandleEnteredForeground();
    /** 1秒服务Ticker检查公开活动窗口，仅有账号/样本时执行；跨边界先通知，再请求刷新。 */
    bool TickBoundaryRefresh(float DeltaSeconds);
    bool HasCrossedCatalogBoundary(
        const FDateTime& PreviousUtc,
        const FDateTime& CurrentUtc) const;

    void HandleCatalogCompleted(
        uint64 ExpectedGeneration,
        uint64 ExpectedRequestGeneration,
        FGamePlatformLiveOpsCatalogSnapshot NewCatalog,
        EGamePlatformLiveOpsError Error);

    void HandlePlayerStateCompleted(
        uint64 ExpectedGeneration,
        uint64 ExpectedRequestGeneration,
        FGamePlatformLiveOpsPlayerState NewState,
        EGamePlatformLiveOpsError Error);

    void HandleClaimCompleted(
        uint64 ExpectedGeneration,
        uint64 ExpectedRequestGeneration,
        FGuid ExpectedOperationId,
        bool bReconcile,
        FGamePlatformLiveOpsClaimResult Result,
        EGamePlatformLiveOpsError Error);

    /** 根据在飞请求和错误提交当前服务状态，再推进通用视图事件；不凭Revision抑制错误/忙碌通知。 */
    void RefreshAggregateState();
};

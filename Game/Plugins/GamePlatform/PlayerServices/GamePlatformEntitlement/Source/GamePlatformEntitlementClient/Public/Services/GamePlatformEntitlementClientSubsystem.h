// 平台LocalPlayer权益服务：Online拥有认证、Transport读取后端权威；游戏线程只读查询/状态事件，账号退出清空投影并取消本Port等待。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Types/GamePlatformEntitlementTypes.h"
#include "Types/GamePlatformEntitlementClientTypes.h"
#include "GamePlatformEntitlementClientSubsystem.generated.h"

class IGamePlatformEntitlementClientTransport;

DECLARE_MULTICAST_DELEGATE(FGamePlatformEntitlementClientChanged);

UCLASS()
class GAMEPLATFORMENTITLEMENTCLIENT_API UGamePlatformEntitlementClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    /** 绑定所属GI Online认证事件，初始已认证时立即装配业务传输；不创建第二认证框架。 */
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    /** 退出先解除Online委托、清空领域事件并取消本Port请求，旧完成不能访问新账号。 */
    virtual void Deinitialize() override;
    /** 游戏线程：受控组合根/测试传入非空账号与Transport；正式默认跟随Online，true仅表示读取受理，不能据此授予权益。 */
    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformEntitlementClientTransport, ESPMode::ThreadSafe> InTransport);

    /** 游戏线程：取消本地账号请求、清空所有权益/派生索引并广播；不注销Online，不回滚后端已提交操作。 */
    void ResetAccount();
    /** 游戏线程：无账号/忙返回false；true表示首次受理，终态通过OnChanged及GetLastError交付，失败保留旧快照。 */
    bool RefreshSnapshot();

    UFUNCTION(BlueprintPure, Category="Entitlement")
    /** 当前本地读取状态；Ready仅表示已接纳快照。 */
    EGamePlatformEntitlementClientState GetState() const { return State; }

    UFUNCTION(BlueprintPure, Category="Entitlement")
    /** 后端快照版本；0尚未加载，不能充当准入授权。 */
    int64 GetSnapshotRevision() const { return Snapshot.Revision; }

    UFUNCTION(BlueprintPure, Category="Entitlement")
    /** 游戏线程只读；None或不存在返回false，仅查询Active投影，不替代服务器授权。 */
    bool HasEntitlement(FName EntitlementId) const;

    /** 游戏线程只读；任意有效身份命中返回true，空数组为false，不修改权益。 */
    bool HasAny(const TArray<FName>& EntitlementIds) const;
    /** 游戏线程只读；全部身份命中返回true，空数组按集合语义为true，不作为权限授予依据。 */
    bool HasAll(const TArray<FName>& EntitlementIds) const;

    UFUNCTION(BlueprintPure, Category="Entitlement")
    /** 游戏线程返回当前派生视图副本；失败保留旧视图，调用方拥有副本。 */
    TArray<FGamePlatformEntitlementViewModel> GetViewModels() const;

    /** 游戏线程原生UI借用缓存只读数组，避免重复构建；引用在下一事件/重置后失效。 */
    const TArray<FGamePlatformEntitlementViewModel>& GetViewModelsView() const
    {
        return CachedViewModels;
    }

    UFUNCTION(BlueprintPure, Category="Entitlement")
    /** 游戏线程按当前Active权益投影查询英雄定义身份；None/未知false，准入仍需服务器校验。 */
    bool IsHeroUnlocked(FName HeroDefinitionId) const;

    UFUNCTION(BlueprintPure, Category="Entitlement")
    /** 游戏线程按当前Active权益投影查询皮肤定义身份；None/未知false，不能解锁权威权益。 */
    bool IsSkinUnlocked(FName SkinDefinitionId) const;

    /** 游戏线程只读借用；引用仅有效到下一变化/重置，不持有跨账号数据。 */
    const FGamePlatformEntitlementSnapshot& GetSnapshot() const
    {
        return Snapshot;
    }

    /** 最近领域错误；失败可仍保留旧投影，不能将缓存当权威授权。 */
    EGamePlatformEntitlementError GetLastError() const { return LastError; }
    /** 首次/Loading/失败/重置/成功都发布，只读消费者通过事件更新，不使用业务Tick。 */
    FGamePlatformEntitlementClientChanged OnChanged;

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
    /** 同账号快照读取代次，阻止上一轮重复响应进入后续读取。 */
    uint64 SnapshotRequestGeneration = 0;
    EGamePlatformEntitlementClientState State =
        EGamePlatformEntitlementClientState::Uninitialized;

    FGamePlatformEntitlementSnapshot Snapshot;

    // 由Snapshot一次构建的只读派生索引；不保存额外业务状态。
    TSet<FName> EffectiveEntitlementIds;
    TSet<FName> EffectiveHeroIds;
    TSet<FName> EffectiveSkinIds;
    TArray<FGamePlatformEntitlementViewModel> CachedViewModels;
    EGamePlatformEntitlementError LastError =
        EGamePlatformEntitlementError::None;

    TSharedPtr<IGamePlatformEntitlementClientTransport, ESPMode::ThreadSafe>
        Transport;

    void RebuildDerivedCaches();

    void HandleSnapshotCompleted(
        uint64 ExpectedGeneration,
        uint64 ExpectedSnapshotRequestGeneration,
        FGamePlatformEntitlementSnapshot NewSnapshot,
        EGamePlatformEntitlementError Error);

    bool ApplySnapshot(
        const FGamePlatformEntitlementSnapshot& NewSnapshot);
};

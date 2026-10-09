#pragma once

// 平台本地玩家成长投影：领域Port提供后端权威快照，客户端不授予XP。
// 所有命令及完成在游戏线程；账号代次过滤迟到响应，缓存仅派生，状态/视图事件驱动UI。

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Types/GamePlatformProgressionClientTypes.h"
#include "GamePlatformProgressionClientSubsystem.generated.h"

class IGamePlatformProgressionClientTransport;
class UGamePlatformProgressionTrackDefinition;

DECLARE_MULTICAST_DELEGATE_FourParams(
    FGamePlatformProgressionLevelChanged,
    FName,
    FString,
    int32,
    int32);

DECLARE_MULTICAST_DELEGATE_FourParams(
    FGamePlatformProgressionXPChanged,
    FName,
    FString,
    int64,
    int64);

DECLARE_MULTICAST_DELEGATE(FGamePlatformProgressionViewChanged);

UCLASS()
class GAMEPLATFORMPROGRESSIONCLIENT_API UGamePlatformProgressionClientSubsystem final
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    /** 作用域退出先解绑事件并取消自身请求；无法撤销已经提交的后端事务。 */
    virtual void Deinitialize() override;
    /** 配置已认证账号及独占生命周期的领域传输；空键/空Port拒绝；true仅表示首次读取已受理，不证明数据就绪。 */
    bool ConfigureAuthenticatedAccount(
        const FString& AccountKey,
        TSharedPtr<IGamePlatformProgressionClientTransport, ESPMode::ThreadSafe>
            InTransport);

    /** 取消本账号请求并清空投影，通知视图；后端已提交事务不因此回滚。 */
    void ResetAccount();
    /** 请求后端成长快照；忙/未配置返回false；完成错误由状态与视图事件给出，旧账号响应不应用。 */
    bool RefreshSnapshot();

    /** 登记已加载且验证有效的曲线定义并保活；不匹配版本保留快照但视图显示不兼容，非法定义忽略。 */
    void RegisterTrackDefinition(
        UGamePlatformProgressionTrackDefinition* Definition);

    UFUNCTION(BlueprintPure, Category="Progression")
    /** 当前本地读取状态；Ready只代表已接纳权威投影。 */
    EGamePlatformProgressionClientState GetState() const
    {
        return State;
    }

    UFUNCTION(BlueprintPure, Category="Progression")
    /** 后端快照版本；0表示尚无快照，与本地事件代次独立。 */
    int64 GetProgressionRevision() const
    {
        return Snapshot.ProgressionRevision;
    }

    UFUNCTION(BlueprintPure, Category="Progression")
    /** 读取指定轨道/主体等级；缺失返回0，不自动创建轨道。 */
    int32 GetLevel(
        FName TrackId,
        const FString& SubjectId) const;

    UFUNCTION(BlueprintPure, Category="Progression")
    /** 读取非负累计XP单位整数；轨道/主体缺失返回0，不写入服务端。 */
    int64 GetTotalXP(
        FName TrackId,
        const FString& SubjectId) const;

    UFUNCTION(BlueprintPure, Category="Progression")
    /** 返回派生视图副本；调用方拥有副本，未兼容曲线不伪造进度。 */
    TArray<FGamePlatformProgressionViewModel> GetViewModels() const;

    /** C++高频UI读取：按Snapshot/Definition代次复用派生ViewModel，避免重复分配和曲线计算。 */
    const TArray<FGamePlatformProgressionViewModel>& GetViewModelsView() const;

    /** 借用当前快照只读引用；下一状态变更/作用域退出后不得继续保存引用。 */
    const FGamePlatformProgressionSnapshot& GetSnapshot() const
    {
        return Snapshot;
    }

    /** 首次/清空/轨道增删/定义变化/加载及错误均通知；读取只读状态，不能把事件视为XP授权。 */
    FGamePlatformProgressionViewChanged OnViewChanged;
    /** 本地派生视图代次，与服务器ProgressionRevision独立，供事件消费者判重。 */
    uint64 GetViewGeneration() const { return ViewGeneration; }
    /** 最近受理/完成错误；None仅代表当前没有记录的错误，不授予任何服务器权限。 */
    EGamePlatformProgressionError GetLastError() const { return LastError; }
    /** 兼容升级事件参数依次为轨道、主体、旧/新等级；首次轨道用OnViewChanged刷新。 */
    FGamePlatformProgressionLevelChanged OnLevelChanged;
    /** 兼容XP变化事件参数依次为轨道、主体、旧/新累计XP；不代表客户端可写XP。 */
    FGamePlatformProgressionXPChanged OnXPChanged;

private:
    FString CurrentAccountKey;
    uint64 AccountGeneration = 0;

    EGamePlatformProgressionClientState State =
        EGamePlatformProgressionClientState::Uninitialized;

    EGamePlatformProgressionError LastError =
        EGamePlatformProgressionError::None;

    FGamePlatformProgressionSnapshot Snapshot;
    uint64 SnapshotGeneration = 0;
    uint64 DefinitionGeneration = 0;
    uint64 ViewGeneration = 0;
    /** 提交完整状态后推进视图代次，再广播；监听者允许ResetAccount，不继续访问旧轨道。 */
    void PublishViewChanged();

    // Derived caches（派生缓存）只由Snapshot/Definition代次驱动，不成为第二份业务真源。
    mutable uint64 CachedIndexSnapshotGeneration = ~uint64(0);
    mutable TMap<FName, TMap<FString, int32>> CachedTrackIndexById;
    mutable uint64 CachedViewSnapshotGeneration = ~uint64(0);
    mutable uint64 CachedViewDefinitionGeneration = ~uint64(0);
    mutable TArray<FGamePlatformProgressionViewModel> CachedViewModels;

    TSharedPtr<IGamePlatformProgressionClientTransport, ESPMode::ThreadSafe>
        Transport;

    UPROPERTY(Transient)
    TMap<FName, TObjectPtr<UGamePlatformProgressionTrackDefinition>>
        Definitions;

    /** 快照代次变更时重建只读索引；保存派生位置，不拥有额外权威数据。 */
    void EnsureTrackIndexCache() const;
    /** 快照或定义代次变更时重建曲线视图，读取无网络或磁盘副作用。 */
    void EnsureViewModelCache() const;

    /** 游戏线程终态入口；代次不匹配丢弃，错误保留现有投影并通知，合法结果一次应用。 */
    void HandleSnapshotCompleted(
        uint64 ExpectedGeneration,
        FGamePlatformProgressionSnapshot NewSnapshot,
        EGamePlatformProgressionError Error);

    /** 拒绝非法/倒退快照；先提交完整投影再发兼容事件，事件重入换账号即停止后续旧轨道访问。 */
    bool ApplySnapshot(
        const FGamePlatformProgressionSnapshot& NewSnapshot);

    const FGamePlatformProgressionTrackState* FindTrack(
        FName TrackId,
        const FString& SubjectId) const;
};

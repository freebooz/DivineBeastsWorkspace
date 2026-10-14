#pragma once

// SSLM专用服务器可信背包事件出口。仅在Dedicated Server游戏实例中创建，
// 项目第三层向后端PlayerData提交经过平台准入验证的采集/任务/使用事实。
// 不承载客户端UI、不存储权威库存、不在引擎模块启动时接入生产接口。

#include "CoreMinimal.h"
#include "Containers/Ticker.h" // FTSTicker 专用句柄与核心心跳时钟，避免误用普通 FDelegateHandle。
#include "Subsystems/GameInstanceSubsystem.h"
#include "DivineBeastsTrustedInventoryEventSubsystem.generated.h"

class APlayerController;
class UGamePlatformServerAdmissionSubsystem;
struct FGamePlatformServerVerifiedAdmission;
class IHttpRequest;
struct FDivineBeastsInventoryPendingRequest;

/** EDivineBeastsInventoryEventOutcome（库存事件结果）：FirstCommitted首次成功、AlreadyCommitted幂等重放、Unknown结果未知。
 * 只有首次提交才可能触发首次业务反馈；重复事件的游戏效果必须由效果消费者按事件ID去重。
 */
enum class EDivineBeastsInventoryEventOutcome : uint8
{
    Unknown,
    FirstCommitted,
    AlreadyCommitted
};

/**
 * UDivineBeastsTrustedInventoryEventSubsystem（项目服务器可信物品事件子系统）。
 * 只有真实服务器玩法完成事件后才由C++调用；任何客户端蓝图/RPC都不直接获得授予入口。
 * 访问PlayerData之前必须确认当前Controller的服务器准入投影、游戏服Boot与会话世代。
 * 发送结果为异步：返回有效FGuid只表示本地请求已创建，不能据此向玩家发放物品或触发技能效果。
 */
UCLASS()
class DBASERVER_API UDivineBeastsTrustedInventoryEventSubsystem final
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    /** 仅Dedicated Server创建，Editor/普通客户端绝不自动装配可信令牌。 */
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** ExpectedInventoryRevision（背包版本提示）可为0：要求后端PlayerData自行读取权威Revision并执行乐观锁；
     * 或由服务器从PlayerData快照取得正数。绝不能直接从客户端自报背包版本。 */
    /** 权威资源点采集成功后提交：SourceEventId必须是该次世界掉落的稳定事件GUID。 */
    FGuid SubmitWorldPickup(const APlayerController& Controller, FGuid SourceEventId,
        FName ItemDefinitionId, int32 Quantity, int64 ExpectedInventoryRevision = 0,
        TFunction<void(EDivineBeastsInventoryEventOutcome)> Completion = {});

    /** 服务器确认任务完成后提交，客户端自报任务完成不得调用此函数。 */
    FGuid SubmitQuestReward(const APlayerController& Controller, FGuid SourceEventId,
        FName ItemDefinitionId, int32 Quantity, int64 ExpectedInventoryRevision = 0,
        TFunction<void(EDivineBeastsInventoryEventOutcome)> Completion = {});

    /** 服务器验证使用资格后提交扣除；玩法效果须等后端确认后由可信Gameplay消费。 */
    FGuid SubmitItemUse(const APlayerController& Controller, FGuid SourceEventId,
        FString ItemInstanceId, FName ItemDefinitionId, int32 Quantity, int64 ExpectedInventoryRevision = 0,
        TFunction<void(EDivineBeastsInventoryEventOutcome)> Completion = {});
    /** 取消本地等待，不能回滚服务端已提交的OperationId；结果未知需同事件ID对账。 */
    bool CancelLocalWait(FGuid RequestId);

private:
    FGuid SubmitConfirmedEvent(const APlayerController& Controller, FGuid SourceEventId,
        const TCHAR* EventKind, const TCHAR* ReasonCode,
        FName DefinitionId, const FString& InstanceId, int32 Quantity, int64 ExpectedInventoryRevision,
        TFunction<void(EDivineBeastsInventoryEventOutcome)> Completion);

    /** 订阅平台原生准入通知，仅保存当前真实Controller弱引用；退出时精确解绑。 */
    void HandleVerifiedAdmissionChanged(const APlayerController* Controller,
        const FGamePlatformServerVerifiedAdmission& Admission, bool bAccepted);
    /** 每10秒遍历活跃准入，不使用每帧Tick；过期/已撤销连接清除。 */
    bool TickPlayerLeaseRenewals(float DeltaSeconds);
    /** 对已验证平台准入发起受限异步30秒续租；失败不伪造后端身份。 */
    void RequestPlayerLeaseRenewal(const APlayerController& Controller,
        const FGamePlatformServerVerifiedAdmission& Admission);
    TWeakObjectPtr<UGamePlatformServerAdmissionSubsystem> BoundAdmissions;
    FDelegateHandle AdmissionChangedHandle;
    /** 10秒续租节拍器拥有的强类型句柄，必须用 FTSTicker::RemoveTicker 成对释放。 */
    FTSTicker::FDelegateHandle LeaseTickerHandle;
    /** 用当前AdmissionId（真实准入身份）关联Controller，防止旧断线回调撤销新连接。 */
    TMap<TWeakObjectPtr<APlayerController>,FGuid> ActiveLeaseControllers;
    TMap<TWeakObjectPtr<APlayerController>,TSharedPtr<IHttpRequest,ESPMode::ThreadSafe>> PendingLeaseRequests;

    // 退出游戏实例先撤销资格及请求，再清理令牌，不清理其他GameInstance的状态。
    TMap<FGuid,TSharedPtr<FDivineBeastsInventoryPendingRequest,ESPMode::ThreadSafe>> Pending;
    FString PlayerDataBaseUrl;
    FString GameID;
    FString GameServerID;
    FString ServerBootID;
    FString InternalBearer;
    uint64 Generation = 0;
    bool bShuttingDown = false;
};

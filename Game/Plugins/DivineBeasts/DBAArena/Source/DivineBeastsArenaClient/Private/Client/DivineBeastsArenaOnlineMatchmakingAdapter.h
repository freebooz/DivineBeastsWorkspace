// 项目客户端真实匹配传输：复用唯一Online认证和预算；初始快照后续接服务器通知，不属于Widget/VM或权威队列。
#pragma once
#include "CoreMinimal.h"
#include "Client/GamePlatformArenaMatchmakingAdapter.h"
#include "Types/GamePlatformOnlineRequests.h"
#include "GamePlatformOnlineClientSubsystem.h"
#include "Containers/Ticker.h"
#include "Flow/DivineBeastsFlowTypes.h"
#include "Client/DivineBeastsArenaClientSubsystem.h"
class UGamePlatformOnlineClientSubsystem;
struct FGamePlatformAuthSnapshot;
class FJsonObject;

/** 后端本人状态的非秘密投影；失败保持bKnown=false，不用旧名单授予按钮资格。 */
struct FDivineBeastsArenaQueueSnapshot
{
    bool bKnown=false,bServiceReady=false,bDevelopment=false,bCanCancel=false,bCanReply=false,bSelfAccepted=false;
    FString Phase,TicketId,PartyId,CandidateId,RegionId,ModeId,ReasonCode;
    int64 Revision=0,CandidateRevision=0;
    FDateTime DeadlineUtc,ServerTimeUtc;
    int32 Participants=0,Accepted=0;
    FDivineBeastsArenaChallengeView Challenge;
};

/** 每GameInstance一条本人观察，多个页面借用；认证撤销先增加代次，再取消在途请求与一次性恢复。 */
class FDivineBeastsArenaOnlineMatchmakingAdapter final:public IGamePlatformArenaMatchmakingAdapter,
    public TSharedFromThis<FDivineBeastsArenaOnlineMatchmakingAdapter>
{
public:
    FDivineBeastsArenaOnlineMatchmakingAdapter(UGamePlatformOnlineClientSubsystem& Online,UObject& Owner);
    ~FDivineBeastsArenaOnlineMatchmakingAdapter() override;
    void Start();
    void Stop();
    /** 人工重试/未知命令恢复时重新建立完整快照，不自动重复业务写入。 */
    void Resynchronize(TFunction<void(bool)> FirstObservation = {});
    const FDivineBeastsArenaQueueSnapshot& GetSnapshot()const{return Snapshot;}
    FGuid GetAuthenticationGeneration()const{return AuthGeneration;}
    FSimpleMulticastDelegate Changed;
    void RequestMatchmaking(const FGamePlatformArenaMatchmakingRequest& Request,TFunction<void(bool,const FString&)> Completion) override;
    void CancelMatchmaking(const FString& ClientRequestId,TFunction<void(bool,const FString&)> Completion) override;
    void RespondToMatchOffer(const FGamePlatformArenaMatchOffer& Offer,bool bAccept,const FString& ClientRequestId,TFunction<void(bool,const FString&)> Completion) override;
    void RequestChallenge(const FGamePlatformArenaMatchmakingRequest& Request,const FString& TargetPlayerId,TFunction<void(bool,const FString&)> Completion);
    void ReplyChallenge(bool bAccept,const FString& ClientRequestId,TFunction<void(bool,const FString&)> Completion);
    void CancelChallenge(const FString& ClientRequestId,TFunction<void(bool,const FString&)> Completion);
    EGamePlatformArenaClientFlowState GetObservedState()const override;
    bool SupportsTicketLifecycle()const override;
    FGamePlatformArenaMatchOffer GetObservedOffer()const override;
    /** 本人已知候选的返程方向。已完成状态只供之前实际分配到竞技场的玩家恢复。 */
    static bool CanRequestReturn(const FDivineBeastsArenaQueueSnapshot& View, bool bPreviousArena);
    /** 票据只作为一次完成参数移动给应用流程，不进入Snapshot/Changed。 */
    TFunction<void()> RequestMigration(const FString& Direction, const FString& ClientRequestId,
        const FString& SourceAssignmentId, TFunction<void(bool, FDivineBeastsWorldAssignmentPayload)> Completion);
    /** 从新鲜本人快照选择重连或赛后返回，复用迁移端点。返回仅撤销本次节点请求的函数。 */
    TFunction<void()> RequestWorldRecovery(const FDivineBeastsWorldAssignmentSummary& Previous,
        TFunction<void(bool, FDivineBeastsWorldAssignmentPayload)> Completion);
private:
    friend class FDivineBeastsArenaBackendCodecTest;
    enum class EMigrationResult : uint8 { Success, Failed, ArenaExpired };
    enum class ERecoveryAction : uint8 { Wait, Fail, Arena, Return };
    struct FMigrationOperation
    {
        FGamePlatformAuthenticatedRequest Request;
        FString CandidateId;
        bool bReturning = false;
        bool bAllowsCompletedReturn = false;
        TFunction<void(EMigrationResult, FDivineBeastsWorldAssignmentPayload)> Completion;
        FGamePlatformOnlineRequestHandle HttpRequest;
        FTSTicker::FDelegateHandle RetryHandle;
        double RetryDeadline = 0.0;
        float RetryDelay = 0.1f;
    };
    struct FWorldRecoveryOperation
    {
        FDivineBeastsWorldAssignmentSummary Previous;
        FString CandidateId;
        FString RequestId;
        double Deadline = 0.0;
        bool bPreviousArena = false;
        bool bWaitingForReturn = false;
        bool bRequestPending = false;
        bool bReturning = false;
        FTSTicker::FDelegateHandle DeadlineHandle;
        TFunction<void()> CancelMigration;
        TFunction<void(bool, FDivineBeastsWorldAssignmentPayload)> Completion;
    };
    static bool IsMigrationError(const FGamePlatformAuthenticatedResponse& Response, const TCHAR* Code);
    static bool IsRetryableArenaMigration(const FGamePlatformAuthenticatedResponse& Response, double Now, double Deadline);
    static double GetMigrationRetryDeadline(const FDivineBeastsArenaQueueSnapshot& View, double Now);
    static ERecoveryAction GetRecoveryAction(const FDivineBeastsArenaQueueSnapshot& View, const FString& CandidateId,
        bool bPreviousArena, bool bWaitingForReturn);
    TFunction<void()> RequestMigrationInternal(const FString& Direction, const FString& RequestId, const FString& SourceAssignmentId,
        TFunction<void(EMigrationResult, FDivineBeastsWorldAssignmentPayload)> Completion, double RecoveryDeadline = 0.0);
    void SendMigrationAttempt(const TSharedRef<FMigrationOperation>& Operation, bool bRetry = false);
    void CancelActiveMigration();
    void AdvanceWorldRecovery();
    void FinishWorldRecovery(bool bSuccess, FDivineBeastsWorldAssignmentPayload Assignment = {});
    void CancelWorldRecovery();
    TSharedPtr<FMigrationOperation> ActiveMigration;
    TSharedPtr<FWorldRecoveryOperation> ActiveRecovery;
    bool DecodeCommandResponse(const FString& Body,const FString& RequestId,FDivineBeastsArenaQueueSnapshot& Out)const;
    void AuthenticationChanged(const FGamePlatformAuthSnapshot& Authentication);
    void Observe(uint64 ExpectedGeneration, TFunction<void(bool)> FirstObservation = {});
    void StopRequests();
    void StopObservation();
    bool DecodeSnapshot(const TSharedPtr<FJsonObject>& Object,FDivineBeastsArenaQueueSnapshot& Out)const;
    bool ApplySnapshot(FDivineBeastsArenaQueueSnapshot Next);
    void SendCommand(TSharedRef<FJsonObject> Body,const FString& RequestId,TFunction<void(bool,const FString&)> Completion);
    /** 快速匹配和约战共用本人队伍身份/版本合同，不复制网络或权威名单规则。 */
    TSharedPtr<FJsonObject> BuildRosterCommand(const TCHAR* Action,const FGamePlatformArenaMatchmakingRequest& Input,FString& OutError) const;
    TWeakObjectPtr<UGamePlatformOnlineClientSubsystem> Online;
    TWeakObjectPtr<UObject> Owner;
    FDelegateHandle AuthHandle;
    FTSTicker::FDelegateHandle RetryHandle;
    FGamePlatformOnlineRequestHandle Observation;
    TArray<FGamePlatformOnlineRequestHandle> Commands;
    FDivineBeastsArenaQueueSnapshot Snapshot;
    FString PlayerId,Cursor,CancelRequestId,CancelTicketId;
    int64 CancelRevision=0;
    FGuid AuthGeneration;
    uint64 Generation=0;
    /** 命令确认时单独失效旧长观察，不能同时取消刚触发的可信迁移。 */
    uint64 ObservationGeneration=0;
    int32 Failures=0;
    bool bStarted=false;
};

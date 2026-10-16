// 项目客户端真实匹配传输：复用唯一Online认证和预算；初始快照后续接服务器通知，不属于Widget/VM或权威队列。
#pragma once
#include "CoreMinimal.h"
#include "Client/GamePlatformArenaMatchmakingAdapter.h"
#include "Types/GamePlatformOnlineRequests.h"
#include "Containers/Ticker.h"
#include "Flow/DivineBeastsFlowTypes.h"
class UGamePlatformOnlineClientSubsystem;
struct FGamePlatformAuthSnapshot;
class FJsonObject;

/** 后端本人状态的非秘密投影；失败保持bKnown=false，不用旧名单授予按钮资格。 */
struct FDivineBeastsArenaQueueSnapshot
{
    bool bKnown=false,bServiceReady=false,bDevelopment=false,bCanCancel=false,bCanReply=false;
    FString Phase,TicketId,PartyId,CandidateId,RegionId,ModeId,ReasonCode;
    int64 Revision=0,CandidateRevision=0;
    FDateTime DeadlineUtc,ServerTimeUtc;
    int32 Participants=0,Accepted=0;
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
    void Resynchronize();
    const FDivineBeastsArenaQueueSnapshot& GetSnapshot()const{return Snapshot;}
    FGuid GetAuthenticationGeneration()const{return AuthGeneration;}
    FSimpleMulticastDelegate Changed;
    void RequestMatchmaking(const FGamePlatformArenaMatchmakingRequest& Request,TFunction<void(bool,const FString&)> Completion) override;
    void CancelMatchmaking(const FString& ClientRequestId,TFunction<void(bool,const FString&)> Completion) override;
    void RespondToMatchOffer(const FGamePlatformArenaMatchOffer& Offer,bool bAccept,const FString& ClientRequestId,TFunction<void(bool,const FString&)> Completion) override;
    EGamePlatformArenaClientFlowState GetObservedState()const override;
    bool SupportsTicketLifecycle()const override;
    FGamePlatformArenaMatchOffer GetObservedOffer()const override;
    /** 票据只作为一次完成参数移动给应用流程，不进入Snapshot/Changed。 */
    void RequestMigration(const FString& Direction,const FString& ClientRequestId,TFunction<void(bool,FDivineBeastsWorldAssignmentPayload)> Completion);
private:
    friend class FDivineBeastsArenaBackendCodecTest;
    bool DecodeCommandResponse(const FString& Body,const FString& RequestId,FDivineBeastsArenaQueueSnapshot& Out)const;
    void AuthenticationChanged(const FGamePlatformAuthSnapshot& Authentication);
    void Observe(uint64 ExpectedGeneration);
    void StopRequests();
    bool DecodeSnapshot(const TSharedPtr<FJsonObject>& Object,FDivineBeastsArenaQueueSnapshot& Out)const;
    bool ApplySnapshot(FDivineBeastsArenaQueueSnapshot Next);
    void SendCommand(TSharedRef<FJsonObject> Body,const FString& RequestId,TFunction<void(bool,const FString&)> Completion);
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
    int32 Failures=0;
    bool bStarted=false;
};

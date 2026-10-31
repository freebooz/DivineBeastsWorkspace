#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Client/GamePlatformArenaClientTypes.h"
#include "DivineBeastsArenaClientSubsystem.generated.h"

class IDivineBeastsApplicationFlowExtension;
class IGamePlatformArenaMatchmakingAdapter;
class FDivineBeastsArenaOnlineMatchmakingAdapter;
class UDivineBeastsApplicationFlowSubsystem;

/** 本人认证代次下的约战只读快照；无准入秘密，不授予客户端名单或比赛权威。 */
struct DIVINEBEASTSARENACLIENT_API FDivineBeastsArenaChallengeView
{
    bool bPresent=false,bCanReply=false,bCanCancel=false;
    FString ChallengeId,OwnerPlayerId,TargetPlayerId,ModeId,RegionId;
    int64 Revision=0;
    FDateTime DeadlineUtc;
    TArray<FString> TeamAPlayerIds,TeamBPlayerIds;
};

/**
 * UDivineBeastsArenaClientSubsystem（神兽联盟竞技客户端适配子系统）。
 * 只做项目模式元数据、ApplicationFlow扩展装配和PostMatch返回；不实现UI/匹配算法/服务器规则。
 */
UCLASS()
class DIVINEBEASTSARENACLIENT_API UDivineBeastsArenaClientSubsystem
    : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintPure, Category="DivineBeasts|Arena")
    TArray<FName> GetProjectArenaModeIds() const;

    /** 事件快照中的队长/人数与模式门禁；只读预检，真正资格仍由后端裁定。 */
    bool CanQueueMode(FName ArenaModeId,FString& OutReason)const;
    bool BuildMatchmakingRequest(
        FName ArenaModeId,
        const FString& PartyId,
        const FString& PreferredRegion,
        const FString& ClientRequestId,
        FGamePlatformArenaMatchmakingRequest& OutRequest,
        FString& OutError) const;

    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Arena")
    bool RequestPostMatchReturnToWorld();

    bool IsApplicationFlowInWorld() const { return bApplicationFlowInWorld; }

    void SetApplicationFlowInWorld(bool bInWorld);
    /** 页面借用同一个真实后端适配端口，关闭页面不停止订阅。 */
    TSharedPtr<IGamePlatformArenaMatchmakingAdapter> GetMatchmakingAdapter()const;
    bool CanCancelObservedQueue()const;
    bool CanReturnObservedMatch()const;
    /** 明确人工重试只重取本人状态/准入，不重复报名、选人或改变服务器资格。 */
    bool RetryObservedMigration();
    bool CanRetryObservedMigration()const;
    FText GetObservedMatchmakingStatus()const;
    /** 当前报名/邀请模式；失联时保留最后活动模式只作保守锁定，不授予写入资格。无活动记录返回None。 */
    FName GetObservedMatchmakingMode() const;
    FDivineBeastsArenaChallengeView GetObservedChallenge() const;
    bool CanChallengeMode(FName ModeId,FString& OutReason) const;
    /** 项目约战意图仍经唯一Online适配器；完成仅代表服务器确认，失败不产生本地旅行。 */
    void RequestChallenge(FName ModeId,const FString& TargetPlayerId,const FString& ClientRequestId,TFunction<void(bool,const FString&)> Completion);
    void ReplyChallenge(bool bAccept,const FString& ClientRequestId,TFunction<void(bool,const FString&)> Completion);
    void CancelChallenge(const FString& ClientRequestId,TFunction<void(bool,const FString&)> Completion);
    FSimpleMulticastDelegate OnMatchmakingStateChanged;
    /** 真实世界/Controller/Pawn复制事件触发；页面不拥有这些订阅，也不查询业务Tick。 */
    void RefreshArenaReadiness();
    bool TryObserveArenaReadiness(UWorld& World,bool& bBound,bool& bPrepared,bool& bActive)const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UDivineBeastsApplicationFlowSubsystem> ApplicationFlow = nullptr;

    TSharedPtr<IDivineBeastsApplicationFlowExtension> FlowExtension;
    bool bApplicationFlowInWorld = false;
    TSharedPtr<FDivineBeastsArenaOnlineMatchmakingAdapter> OnlineMatchmaking;
    FDelegateHandle MatchmakingChangedHandle,PartyChangedHandle;
    TWeakObjectPtr<class UGamePlatformPartyClientSubsystem> PartySource;
    /** 只保存逻辑意图身份；票据只在回调栈移动给Flow。 */
    FString MigrationCandidateId,MigrationDirection,MigrationRequestId;
    FGuid MigrationAuthGeneration;
    uint64 MigrationGeneration=0;
    bool bMigrationPending=false;
    bool bDeinitializing=false;
    FText MigrationError;
    void HandleMatchmakingChanged();
    bool RequestObservedMigration(const FString& Direction);

    /** 以下弱引用及委托只属于当前World；旅行/销毁先解绑，再接受新世界事实。 */
    TWeakObjectPtr<UWorld> ReadinessWorld;
    TWeakObjectPtr<APlayerController> ReadinessController;
    TWeakObjectPtr<class AGamePlatformArenaGameState> ReadinessGameState;
    TWeakObjectPtr<class UDivineBeastsCharacterComponent> ReadinessCharacter;
    TWeakObjectPtr<class UGamePlatformGameplayEligibilityComponent> ReadinessEligibility;
    FDelegateHandle ReadinessMapHandle,ReadinessCleanupHandle,ReadinessGameStateHandle;
    void ClearArenaReadiness();
    UFUNCTION() void HandleReadinessPawnChanged(APawn* OldPawn,APawn* NewPawn);

};

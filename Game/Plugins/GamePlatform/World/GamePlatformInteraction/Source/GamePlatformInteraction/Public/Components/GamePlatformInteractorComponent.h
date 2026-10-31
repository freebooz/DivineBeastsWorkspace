#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Types/GamePlatformInteractionEvent.h"
#include "Types/GamePlatformInteractionFocusSnapshot.h"
#include "Types/GamePlatformInteractionRequest.h"
#include "Types/GamePlatformInteractionSession.h"
#include "GamePlatformInteractorComponent.generated.h"

class UGamePlatformInteractableComponent;
class APawn;
class AController;
class APlayerController;
class UWorld;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformInteractionFocusChanged,
    const FGamePlatformInteractionFocusSnapshot&, Focus);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformInteractionSessionChanged,
    const FGamePlatformInteractionSession&, Session);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformInteractionResultChanged,
    const FGamePlatformInteractionResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformInteractionEventDelegate,
    const FGamePlatformInteractionEvent&, Event);

/**
 * 玩家拥有Actor上的交互发起组件。
 * Focus完全本地；Begin/Cancel为低频可靠Server RPC，服务器重新验证所有权威事实。
 * 成功事实另外通过拥有者可靠Client RPC投影到同一OnInteractionEvent；客户端通知不授予业务权威。
 */
UCLASS(ClassGroup=(GamePlatform), meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMINTERACTION_API UGamePlatformInteractorComponent final
    : public UActorComponent
{
    GENERATED_BODY()

public:
    UGamePlatformInteractorComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintCallable, Category="Interaction")
    void RefreshLocalFocus();
    /** 游戏线程本地显示策略：Enabled=false撤销焦点采样，UsePointer=true使用鼠标射线。
     * 只影响提示/候选，不复制、不修改服务器资格；UI激活/失活时调用，不需要Widget Tick。 */
    void SetLocalFocusPolicy(bool bEnabled, bool bUsePointer);
    /** 当前本地集中采样是否运行，仅供只读诊断；所有权事件控制启停，不代表服务器交互资格。 */
    bool IsLocalFocusSamplingActive() const;

    UFUNCTION(BlueprintCallable, Category="Interaction")
    FGuid BeginFocusedInteraction();

    UFUNCTION(BlueprintCallable, Category="Interaction")
    void CancelCurrentInteraction();

    UFUNCTION(BlueprintPure, Category="Interaction")
    const FGamePlatformInteractionFocusSnapshot& GetCurrentFocus() const
    {
        return CurrentFocus;
    }

    UFUNCTION(BlueprintPure, Category="Interaction")
    const FGamePlatformInteractionSession& GetCurrentSession() const
    {
        return CurrentSession;
    }

    UFUNCTION(BlueprintPure, Category="Interaction")
    const FGamePlatformInteractionResult& GetLastResult() const
    {
        return LastResult;
    }

    UFUNCTION(BlueprintPure, Category="Interaction")
    float GetHoldProgress() const;

    /** Target EndPlay/Generation变化时由同插件目标组件直接调用。 */
    void CancelFromTarget(
        const FGuid& SessionId,
        EGamePlatformInteractionCancelReason Reason);

    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FGamePlatformInteractionFocusChanged OnFocusChanged;

    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FGamePlatformInteractionSessionChanged OnSessionChanged;

    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FGamePlatformInteractionResultChanged OnResultChanged;

    /** 服务器本地事件；拥有者客户端只接收与本地已发送请求匹配的成功终态，不依赖结果属性复制时序。 */
    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FGamePlatformInteractionEventDelegate OnInteractionEvent;

protected:
    UFUNCTION(Server, Reliable)
    void ServerRequestBeginInteraction(
        FGamePlatformInteractionRequest Request);

    UFUNCTION(Server, Reliable)
    void ServerRequestCancelInteraction(FGuid RequestId);

    /** 服务器到拥有连接的一次性成功快照。与Session/LastResult属性复制独立，不修改客户端业务状态。
     * Recipient为服务器在终态监听前捕获的拥有关系；接收端还需匹配自己发送时的请求与作用域。 */
    UFUNCTION(Client, Reliable)
    void ClientReceiveCommittedInteraction(
        FGamePlatformInteractionEvent Event,
        FGamePlatformInteractionSession Session,
        APlayerController* RecipientController,
        APawn* RecipientPawn);

private:
    // 自动化用例直接驱动真实终态边界；不为生产调用方暴露绕过准入的入口。
    friend class FInteractionTerminalReentryTest;
    friend struct FInteractionClientCommitTestAccess;
    /** BeginPlay及真实拥有关系事件重评估焦点定时器；失去本地拥有者时立即清空本地焦点。 */
    void ReconcileLocalFocusSampling();
    UFUNCTION()
    void HandleOwnerControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);
    UFUNCTION()
    void HandleOwnerPawnChanged(APawn* OldPawn, APawn* NewPawn);
    struct FCachedRequestResult
    {
        FGamePlatformInteractionResult Result;
        double RecordedAt = 0.0;
    };

    /** 客户端发送记录不拥有Actor；ScopeId隔离失活/拥有关系换代，身份字段只用于拒绝错配通知。 */
    struct FLocalNotificationRequest
    {
        TWeakObjectPtr<AActor> TargetActor;
        FGuid TargetInstanceId;
        int32 TargetGeneration = 0;
        int32 TargetRevision = 0;
        FName OptionId = NAME_None;
        FGuid ScopeId;
        bool bDelivered = false;
    };
    /** 游戏线程接收作用域与最多64份发送/去重记录；不复制、不定时扫描、不拥有业务状态。 */
    FGuid LocalNotificationScopeId;
    TWeakObjectPtr<UWorld> LocalNotificationWorld;
    TWeakObjectPtr<APlayerController> LocalNotificationController;
    TWeakObjectPtr<APawn> LocalNotificationPawn;
    TMap<FGuid, FLocalNotificationRequest> LocalNotificationRequests;
    TArray<FGuid> LocalNotificationRequestOrder;
    TSet<FGuid> DeliveredNotificationSessions;
    TArray<FGuid> DeliveredNotificationSessionOrder;
    bool bAcceptLocalNotifications = false;

    UPROPERTY(Transient)
    FGamePlatformInteractionFocusSnapshot CurrentFocus;

    UPROPERTY(ReplicatedUsing=OnRep_CurrentSession)
    FGamePlatformInteractionSession CurrentSession;

    UPROPERTY(ReplicatedUsing=OnRep_LastResult)
    FGamePlatformInteractionResult LastResult;

    FTimerHandle FocusTimer;
    /** 默认保留平台既有中心射线策略；项目可按自身输入方式显式选择鼠标焦点。 */
    bool bLocalFocusEnabled = true;
    bool bUsePointerFocus = false;
    FTimerHandle HoldValidationTimer;

    TMap<FGuid, FCachedRequestResult> RecentRequestResults;
    TArray<FGuid> RecentRequestOrder;

    double BeginWindowStart = 0.0;
    int32 BeginRequestCount = 0;
    double CancelWindowStart = 0.0;
    int32 CancelRequestCount = 0;

    double LastLocalBeginRequestTime = -1.0;
    double LastLocalCancelRequestTime = -1.0;

    UFUNCTION()
    void OnRep_CurrentSession();

    UFUNCTION()
    void OnRep_LastResult();

    bool IsLocallyControlledOwner() const;
    bool ResolveServerGameplayEligibility(
        int32& OutInteractorGeneration) const;
    bool ResolveExtraEligibility() const;

    FVector GetServerInteractorOrigin() const;
    bool ValidateServerDistance(
        const UGamePlatformInteractableComponent& Target,
        const FGamePlatformInteractionOption& Option) const;
    bool ValidateServerLineOfSight(
        const UGamePlatformInteractableComponent& Target,
        const FGamePlatformInteractionOption& Option) const;

    EGamePlatformInteractionError ValidateBeginRequest(
        const FGamePlatformInteractionRequest& Request,
        UGamePlatformInteractableComponent*& OutTarget,
        FGamePlatformInteractionOption& OutOption,
        int32& OutInteractorGeneration) const;

    EGamePlatformInteractionError ValidateActiveSession() const;

    void StartSession(
        const FGamePlatformInteractionRequest& Request,
        UGamePlatformInteractableComponent& Target,
        const FGamePlatformInteractionOption& Option,
        int32 InteractorGeneration);

    void CommitCurrentSession();
    void ValidateHoldSession();

    void CancelSession(
        EGamePlatformInteractionCancelReason Reason,
        EGamePlatformInteractionError Error);

    void FinishSession(
        const FGamePlatformInteractionResult& Result,
        EGamePlatformInteractionEventType EventType);

    void PublishEvent(
        EGamePlatformInteractionEventType EventType,
        const FGamePlatformInteractionResult& Result,
        const FGamePlatformInteractionSession& Session,
        APlayerController* RecipientController = nullptr,
        APawn* RecipientPawn = nullptr);

    /** 失活/拥有关系/EndPlay即时撤销旧请求；不回放历史成功。只在生命周期事件和命令边界调用。 */
    void ResetLocalNotificationScope();
    void ReconcileLocalNotificationScope();
    /** 在实际Server RPC发送前保存原始请求；有界淘汰后的迟到消息安全丢弃，不猜测目标。 */
    void RememberLocalRequest(const FGamePlatformInteractionRequest& Request);

    bool ConsumeLocalRequestThrottle(bool bBeginRequest);
    bool ConsumeRequestRateLimit(bool bBeginRequest);
    void PruneRecentRequests();
    void CacheTerminalResult(const FGamePlatformInteractionResult& Result);

    UGamePlatformInteractableComponent* GetCurrentTargetComponent() const;
    bool IsSessionActive() const;
};

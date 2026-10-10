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

    UPROPERTY(BlueprintAssignable, Category="Interaction")
    FGamePlatformInteractionEventDelegate OnInteractionEvent;

protected:
    UFUNCTION(Server, Reliable)
    void ServerRequestBeginInteraction(
        FGamePlatformInteractionRequest Request);

    UFUNCTION(Server, Reliable)
    void ServerRequestCancelInteraction(FGuid RequestId);

private:
    // 自动化用例直接驱动真实终态边界；不为生产调用方暴露绕过准入的入口。
    friend class FInteractionTerminalReentryTest;
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

    UPROPERTY(Transient)
    FGamePlatformInteractionFocusSnapshot CurrentFocus;

    UPROPERTY(ReplicatedUsing=OnRep_CurrentSession)
    FGamePlatformInteractionSession CurrentSession;

    UPROPERTY(ReplicatedUsing=OnRep_LastResult)
    FGamePlatformInteractionResult LastResult;

    FTimerHandle FocusTimer;
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
        const FGamePlatformInteractionSession& Session);

    bool ConsumeLocalRequestThrottle(bool bBeginRequest);
    bool ConsumeRequestRateLimit(bool bBeginRequest);
    void PruneRecentRequests();
    void CacheTerminalResult(const FGamePlatformInteractionResult& Result);

    UGamePlatformInteractableComponent* GetCurrentTargetComponent() const;
    bool IsSessionActive() const;
};

// 本文件属于MobaCommon可选MOBA层 MobaPresentation，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformPresentationTypes.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Types/MobaPresentationTypes.h"
#include "TimerManager.h"
#include "MobaPresentationClientSubsystem.generated.h"

class AGamePlatformArenaGameState;
class AGamePlatformArenaPlayerState;
class UGamePlatformCombatComponent;
class UWorld;
class IMobaPresentationContextContributor;
class APlayerController;
class APawn;
class AActor;
class AGameStateBase;

/**
 * UMobaPresentationClientSubsystem（MOBA客户端表现适配子系统）。
 * LocalPlayer作用域保证Multi-PIE隔离；只消费事实并提交平台表现请求，不拥有Gameplay权威。
 */
UCLASS()
class MOBAPRESENTATIONCLIENT_API UMobaPresentationClientSubsystem
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    void RefreshBindings();

    EGamePlatformPresentationSubmitResult SubmitAdaptedFact(
        FMobaPresentationAdaptedFact Fact);

    void AdaptCombatEvent(const FGamePlatformCombatEvent& Event);
    EGamePlatformPresentationSubmitResult AdaptCriticalFact(
        const FMobaPresentationCriticalFact& Fact);
    EGamePlatformPresentationSubmitResult AdaptAbilityFact(
        const FMobaPresentationAbilityFact& Fact);
    EGamePlatformPresentationSubmitResult AdaptStatusFact(
        const FMobaPresentationStatusFact& Fact);
    EGamePlatformPresentationSubmitResult AdaptCharacterFact(
        const FMobaPresentationCharacterFact& Fact);
    EGamePlatformPresentationSubmitResult AdaptArenaFact(
        const FMobaPresentationArenaFact& Fact);

    /** Late Join只允许恢复持续状态；瞬时Hit/Cast/Death等不会重播。 */
    EGamePlatformPresentationSubmitResult RecoverPersistentFact(
        FMobaPresentationAdaptedFact Fact);

    bool RegisterContextContributor(
        FName ContributorId,
        TSharedRef<IMobaPresentationContextContributor> Contributor);
    bool UnregisterContextContributor(FName ContributorId);

    int32 GetWorldGeneration() const { return WorldGeneration; }
    int32 GetDuplicateDropCount() const { return DuplicateDropCount; }
    int32 GetStaleDropCount() const { return StaleDropCount; }
    int32 GetProviderMissingCount() const { return ProviderMissingCount; }

private:
    friend class FMobaPresentationPawnBindingRegressionTest;
    struct FPlayerSnapshot
    {
        int32 StatsRevision = 0;
        int32 Score = 0;
        int32 ObjectiveScore = 0;
    };

    void ResetWorldState(UWorld* NewWorld);
    void UnbindArena();
    void UnbindCombat();
    /** 本地玩家与World事件接线；关停/旅行先解绑，回调只刷新当前所属世界。 */
    void BindWorldEvents(UWorld* World);
    void UnbindWorldEvents();
    void BindController(APlayerController* Controller);
    void HandleControllerChanged(APlayerController* Controller);
    void HandlePawnChanged(APawn* Pawn);
    void HandleGameStateSet(AGameStateBase* GameState);
    void HandleActorSpawned(AActor* Actor);
    /** 延后出生接线只接受同服务/World世代；已关闭或旅行后的回调不重新订阅。 */
    void HandleDeferredBindingRefresh(uint64 ExpectedGeneration, TWeakObjectPtr<UWorld> ExpectedWorld);
    void BindArena(AGamePlatformArenaGameState* GameState);
    void RefreshArenaPlayerBindings();

    void HandlePostLoadMap(UWorld* LoadedWorld);
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    void HandleArenaPhaseChangedTyped(int32 Revision);
    void HandleArenaTeamStatesChanged();
    void HandleArenaObjectiveStatesChanged();
    void HandleArenaResultChanged();
    void HandleArenaPlayerStatsChanged(AGamePlatformArenaPlayerState* PlayerState);

    UFUNCTION()
    void HandleCombatEvent(const FGamePlatformCombatEvent& Event);

    bool PrepareFact(FMobaPresentationAdaptedFact& Fact);
    bool RememberFact(const FMobaPresentationFactIdentity& Identity);
    void ApplyContextContributors(FMobaPresentationContext& Context) const;
    FGuid MakeArenaFactId(const FString& Scope, int32 Revision, uint32 Salt = 0) const;

    TWeakObjectPtr<UWorld> BoundWorld;
    TWeakObjectPtr<AGamePlatformArenaGameState> BoundArenaGameState;
    TWeakObjectPtr<UGamePlatformCombatComponent> BoundCombatComponent;
    TWeakObjectPtr<APlayerController> BoundController;
    FDelegateHandle PlayerControllerChangedHandle;
    FDelegateHandle PawnChangedHandle;
    FDelegateHandle GameStateSetHandle;
    FDelegateHandle ActorSpawnedHandle;
    /** 下一调度轮查找的唯一计时器，解绑所属World时取消，不持有World强引用。 */
    FTimerHandle PendingBindingRefreshTimer;
    uint64 BindingGeneration = 1;
    bool bClosing = false;

    FDelegateHandle PostLoadMapHandle;
    FDelegateHandle WorldCleanupHandle;
    FDelegateHandle ArenaPhaseHandle;
    FDelegateHandle ArenaTeamsHandle;
    FDelegateHandle ArenaObjectivesHandle;
    FDelegateHandle ArenaResultHandle;

    TMap<TWeakObjectPtr<AGamePlatformArenaPlayerState>, FDelegateHandle> ArenaPlayerHandles;
    TMap<TWeakObjectPtr<AGamePlatformArenaPlayerState>, FPlayerSnapshot> ArenaPlayerSnapshots;
    TMap<FName, int32> TeamRevisions;
    TMap<FName, int32> ObjectiveRevisions;

    TMap<FName, TSharedPtr<IMobaPresentationContextContributor>> ContextContributors;

    TSet<FGuid> PredictedFacts;
    TSet<FGuid> ConfirmedFacts;
    TArray<FGuid> FactOrder;
    TMap<FString, int32> LatestAvatarGeneration;

    int32 WorldGeneration = 1;
    int32 RequestGeneration = 0;
    int32 LastPhaseRevision = INDEX_NONE;
    int32 DuplicateDropCount = 0;
    int32 StaleDropCount = 0;
    int32 ProviderMissingCount = 0;
    int32 PredictedConfirmationCount = 0;

    static constexpr int32 MaxRememberedFacts = 2048;
};

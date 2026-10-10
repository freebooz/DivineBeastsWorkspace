// 本文件属于MobaCommon可选MOBA层 MobaPresentation，负责对外稳定合同/值类型；所属线程、空值、代次和所有权按相邻说明。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformPresentationTypes.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Types/MobaPresentationTypes.h"
#include "TimerManager.h"
#include "Feedback/MobaHitFeedbackPolicy.h"
#include "MobaPresentationClientSubsystem.generated.h"

class AGamePlatformArenaGameState;
class AGamePlatformArenaPlayerState;
class UGamePlatformCombatComponent;
class UGamePlatformCombatFeedbackWorldSubsystem;
class UGamePlatformHitFeedbackProfile;
class UWorld;
class IMobaPresentationContextContributor;
class APlayerController;
class APawn;
class AActor;
class AGameStateBase;

/**
 * MOBA通用的每次命中已加载表现配置。Resolver只提供数据，不加载资产、不控制服务器结果。
 * 生命周期归上层本地玩家组合根；Profile在此不拥有资产租约。
 */
struct FMobaResolvedHitFeedbackConfiguration
{
    TWeakObjectPtr<UGamePlatformHitFeedbackProfile> LoadedProfile;
    FName VFXDefinitionId = NAME_None;
    FName SFXDefinitionId = NAME_None;
};

/** 由可选第三层组合根提供的同步已加载缓存查询，不允许在回调内同步加载。 */
using FMobaHitFeedbackResolver = TFunction<bool(
    const FGamePlatformCombatEvent&, FMobaResolvedHitFeedbackConfiguration&)>;

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

    /** GT事件驱动刷新当前World/Controller/Pawn；外部事实恢复重入换代后旧绑定栈立即停止。 */
    void RefreshBindings();

    /**
     * GT提交值事实；贡献者/Provider可同步注销、关闭或旅行，失效返回InvalidRequest/StaleWorld。
     * Submitted仅指已有或本次真正受理；同ID同步在途重入返回Pending，不能视为播放成功。
     * 预测Provider内到达的首份确认由原栈留存：预测受理后只升级，拒绝后执行真实确认提交。
     * 原同步栈结束前不递归播放；结束后调用方可同ID查询/重试实际终态，旧作用域不清后继账本。
     */
    EGamePlatformPresentationSubmitResult SubmitAdaptedFact(
        FMobaPresentationAdaptedFact Fact);

    void AdaptCombatEvent(const FGamePlatformCombatEvent& Event);
    /**
     * 根据权威攻击类别执行客户端视觉顿帧，不修改真实硬直、伤害或位移。
     */
    void ApplyVisualFeedbackForConfirmedHit(
        const FGamePlatformCombatEvent& Event,
        EMobaHitFeedbackContact Contact,
        int32 ComboStep);

    /** 注入可配置中立反馈参数，供竞技组合根使用。 */
    void ConfigureHitFeedback(const FGamePlatformHitFeedbackTuning& Tuning)
    {
        if (bClosing) return;
        ++HitFeedbackConfigurationGeneration;
        HitFeedbackTuning = Tuning;
    }

    /**
     * 竞技组合根注入已完成GamePlatformData租约加载的Profile与目录逻辑ID。
     * 未预加载的软资源不得在命中关键路径同步加载；允许单独禁用VFX或SFX。
     */
    void ConfigureHitFeedbackProfile(
        UGamePlatformHitFeedbackProfile* LoadedProfile,
        FName VFXDefinitionId,
        FName SFXDefinitionId);

    /** GT同步按命中Hero/Ability查已完成租约；未命中只用中立参数，不复用其他英雄的Profile/VFX/SFX。 */
    void SetHitFeedbackResolver(FMobaHitFeedbackResolver&& Resolver)
    {
        if (bClosing) return;
        ++HitFeedbackConfigurationGeneration;
        HitFeedbackResolver = MoveTemp(Resolver);
    }

    /** 组合根卸载时移除闭包，防止下一世界访问旧角色/内容包。 */
    void ClearHitFeedbackResolver() { ++HitFeedbackConfigurationGeneration; HitFeedbackResolver = {}; }
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

    /** GT注册同步只读扩展器；注册表持有SharedRef，调用栈另持值拷贝，支持回调内自注销。 */
    bool RegisterContextContributor(
        FName ContributorId,
        TSharedRef<IMobaPresentationContextContributor> Contributor);
    /** GT撤销此ID后续扩展；正在执行的原对象仍由本次调用持有至返回，移除成功返回true。 */
    bool UnregisterContextContributor(FName ContributorId);

    int32 GetWorldGeneration() const { return WorldGeneration; }
    int32 GetDuplicateDropCount() const { return DuplicateDropCount; }
    int32 GetStaleDropCount() const { return StaleDropCount; }
    int32 GetProviderMissingCount() const { return ProviderMissingCount; }

private:
    friend class FMobaPresentationPawnBindingRegressionTest;
    friend class FMobaPresentationArenaArrayReentryTest;
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
    /** 订阅当前客户端世界已确认事实，不扫描或缓存全场战斗Actor。 */
    void BindCombatFeedbackWorld(UWorld& World);
    void UnbindCombatFeedbackWorld();
    void HandleConfirmedNetworkCombatEvent(const FGamePlatformCombatEvent& Event);
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

    /** 外部贡献回调后必须仍属原作用域；不把旧事实自动转交回调建立的后继World。 */
    bool PrepareFact(FMobaPresentationAdaptedFact& Fact, TFunctionRef<bool()> IsCurrentScope);
    /** 只登记已经真实Submitted的事实或升级已受理预测；从不把在途预约写入受理集合。 */
    bool RememberFact(const FMobaPresentationFactIdentity& Identity, uint64 OperationGeneration);
    bool ApplyContextContributors(FMobaPresentationContext& Context, TFunctionRef<bool()> IsCurrentScope) const;
    bool IsArenaBindingCurrent(uint64 ExpectedBinding, uint64 ExpectedArenaBinding,
        AGamePlatformArenaGameState* ExpectedState, UWorld* ExpectedWorld) const;
    FGuid MakeArenaFactId(const FString& Scope, int32 Revision, uint32 Salt = 0) const;

    /** 当前本地玩家唯一可选项目Resolver，不拥有其返回的资源。 */
    FMobaHitFeedbackResolver HitFeedbackResolver;
    /** 同步Resolver或Provider可更换配置；旧调用栈不能继续使用上一份Profile/目录身份。 */
    uint64 HitFeedbackConfigurationGeneration = 1;
    /** Resolver阶段也拒绝同EventId嵌套，结束后撤销临时资格，正常去重仍由Rendered集合持有。 */
    TSet<FGuid> ResolvingHitEvents;

    // 本地玩家独立调校数据，不由表现参数更改服务器判定。
    FGamePlatformHitFeedbackTuning HitFeedbackTuning;
    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformHitFeedbackProfile> LoadedHitFeedbackProfile = nullptr;
    FName HitVFXDefinitionId = NAME_None;
    FName HitSFXDefinitionId = NAME_None;

    /** 分层VFX/SFX/闪白/镜头共用命中事实去重，不会各自重复播放。 */
    TSet<FGuid> RenderedHitEvents;
    TArray<FGuid> RenderedHitOrder;
    static constexpr int32 MaxRenderedHitEvents = 2048;

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
    /** 同World的GameState也能替换，竞技批处理必须另核原GameState绑定操作身份。 */
    uint64 ArenaBindingGeneration = 1;
    bool bClosing = false;
    TWeakObjectPtr<UGamePlatformCombatFeedbackWorldSubsystem> BoundCombatWorldBus;
    FDelegateHandle CombatWorldFeedbackHandle;

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

    /** 原同步Submit栈持强引用；注册表换代时清除资格，但不销毁正在执行的本地操作。 */
    struct FPendingFactSubmission
    {
        uint64 OperationGeneration = 0;
        bool bPredicted = false;
        // 只留首份同身份确认值快照；重复回调不增加队列，也不借用Provider的输入引用。
        TOptional<FMobaPresentationAdaptedFact> Confirmation;
    };
    TMap<FGuid, TSharedPtr<FPendingFactSubmission>> PendingFactSubmissions;
    /** 只包含平台已受理事实；在途预约独立保存，不能升级成伪成功。 */
    TSet<FGuid> PredictedFacts;
    TSet<FGuid> ConfirmedFacts;
    TArray<FGuid> FactOrder;
    /** 当前每个FactId账本的唯一写入操作；计数跨World单调推进，清作用域只清记录。 */
    TMap<FGuid, uint64> FactRecordOperations;
    uint64 FactOperationGeneration = 0;
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

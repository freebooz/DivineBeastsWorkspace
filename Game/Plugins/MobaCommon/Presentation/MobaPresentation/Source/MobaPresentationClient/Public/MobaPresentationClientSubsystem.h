#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformPresentationTypes.h"
#include "Types/GamePlatformCombatEvent.h"
#include "Types/MobaPresentationTypes.h"
#include "Feedback/MobaHitFeedbackPolicy.h"
#include "MobaPresentationClientSubsystem.generated.h"

class AGamePlatformArenaGameState;
class AGamePlatformArenaPlayerState;
class UGamePlatformCombatComponent;
class UGamePlatformCombatFeedbackWorldSubsystem;
class UGamePlatformHitFeedbackProfile;
class UWorld;
class IMobaPresentationContextContributor;

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

    void RefreshBindings();

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

    /** 可选项目层解析器按命中Hero/Ability查找已完成的租约，失败安全回退默认配置。 */
    void SetHitFeedbackResolver(FMobaHitFeedbackResolver&& Resolver)
    {
        HitFeedbackResolver = MoveTemp(Resolver);
    }

    /** 组合根卸载时移除闭包，防止下一世界访问旧角色/内容包。 */
    void ClearHitFeedbackResolver() { HitFeedbackResolver = {}; }
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
    struct FPlayerSnapshot
    {
        int32 StatsRevision = 0;
        int32 Score = 0;
        int32 ObjectiveScore = 0;
    };

    void ResetWorldState(UWorld* NewWorld);
    void UnbindArena();
    void UnbindCombat();    /** 订阅当前客户端世界已确认事实，不扫描或缓存全场战斗Actor。 */
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

    bool PrepareFact(FMobaPresentationAdaptedFact& Fact);
    bool RememberFact(const FMobaPresentationFactIdentity& Identity);
    void ApplyContextContributors(FMobaPresentationContext& Context) const;
    FGuid MakeArenaFactId(const FString& Scope, int32 Revision, uint32 Salt = 0) const;

    /** 当前本地玩家唯一可选项目Resolver，不拥有其返回的资源。 */
    FMobaHitFeedbackResolver HitFeedbackResolver;

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

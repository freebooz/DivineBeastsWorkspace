#pragma once
#include "Engine/TimerHandle.h" // 私有适配器持有真实计时器句柄，不依赖调用文件隐式包含。

#include "CoreMinimal.h"
#include "Engine/TimerHandle.h"
#include "Arena/GamePlatformArenaPolicies.h"
#include "Types/GamePlatformDataLease.h"

class UWorld;
class ADivineBeastsCharacter;
struct FGamePlatformCombatEvent;
class AGamePlatformArenaGameMode;
class AGamePlatformArenaPlayerController;
class AGamePlatformArenaPlayerState;

/**
 * FDivineBeastsArenaGameplayLifecycleAdapter（神兽联盟竞技玩法生命周期适配器）。
 *
 * 边界：
 * - Arena 只决定何时出生/复活；
 * - 本适配器只调用 UE GameMode 标准 RestartPlayerAtPlayerStart，不直接 SpawnActor / Possess；
 * - 角色项目初始化统一委托 GamePlatformCharacter 的 InitializationExecutor；
 * - Hero Definition 未预热或初始化未 Ready 时 Fail Closed，比赛不得进入 InProgress。
 * - 每个GameMode独占适配器、出生代次和复活定时器；销毁时取消自身定时器，不影响其他世界。
 */
class FDivineBeastsArenaGameplayLifecycleAdapter final
    : public IGamePlatformArenaGameplayLifecycleAdapter
    , public TSharedFromThis<FDivineBeastsArenaGameplayLifecycleAdapter>
{
public:
    explicit FDivineBeastsArenaGameplayLifecycleAdapter(
        AGamePlatformArenaGameMode& InGameMode, const TArray<FGamePlatformDataLease>& WarmupLeases);
    virtual ~FDivineBeastsArenaGameplayLifecycleAdapter() override;

    virtual bool CapturePlayerGameplayOwnership(const FString& PlayerId, FGamePlatformArenaGameplayOwnership& OutOwnership) const override;

    virtual bool SpawnPlayer(
        const FString& PlayerId,
        FName TeamId,
        FName SpawnPolicyId,
        FString& OutReason) override;

    virtual bool SetPlayerGameplayActive(const FString& PlayerId, bool bActive, FString& OutReason) override;

    /** true仅代表自有Timer受理；零秒也NextTick，后续真实出生失败明确记录且保持Inactive。 */
    virtual bool RequestRespawn(
        const FString& PlayerId,
        FName TeamId,
        FName RespawnPolicyId,
        float DelaySeconds,
        FString& OutReason) override;

private:
    friend class FDivineBeastsArenaDeathBridgeTest;
    /** 每个当前Pawn的中立死亡订阅由本世界Adapter拥有，替换/断线/结束先解绑，不影响其他装配。 */
    struct FDeathBinding { TWeakObjectPtr<ADivineBeastsCharacter> Pawn; FDelegateHandle Handle; };
    void BindPawnDeath(const FString& PlayerId, ADivineBeastsCharacter& Pawn, AGamePlatformArenaPlayerState& State, int32 AvatarGeneration);
    void UnbindPawnDeath(const FString& PlayerId);
    /** 只桥接受信目标死亡。RelatedPlayerId保持空，击杀/助攻/目标归因须另有批准合同。false明确未接纳。 */
    bool HandlePawnDeath(const FString& PlayerId, TWeakObjectPtr<ADivineBeastsCharacter> Pawn,
        TWeakObjectPtr<AGamePlatformArenaPlayerState> State, FString MatchId, int32 AvatarGeneration,
        const FGamePlatformCombatEvent& Event);
    TMap<FString, FDeathBinding> DeathBindings;
    bool ResolvePlayer(
        const FString& PlayerId,
        AGamePlatformArenaPlayerController*& OutController,
        AGamePlatformArenaPlayerState*& OutPlayerState,
        FString& OutReason) const;

    /** 每个Timer捕获原世界/比赛/连接/Pawn及代次，RequestId区分同玩家后续请求；借用弱引用，无资源所有权。 */
    struct FRespawnRequest
    {
        FTimerHandle Timer;
        FGuid RequestId;
        FGuid BindingId;
        TWeakObjectPtr<UWorld> World;
        TWeakObjectPtr<AGamePlatformArenaPlayerController> Controller;
        TWeakObjectPtr<AGamePlatformArenaPlayerState> State;
        TWeakObjectPtr<APawn> Pawn;
        bool bHadPawn = false;
        FString MatchId;
        FString ServerId;
        FString CharacterId;
        FName TeamId;
        FName PolicyId;
        int32 AvatarGeneration = 0;
    };
    bool CaptureRespawnContext(const FString& PlayerId, FName TeamId, FName PolicyId, FRespawnRequest& OutContext) const;
    bool IsRespawnContextCurrent(const FString& PlayerId, const FRespawnRequest& Context) const;
    /** false表示过期/撤销或出生失败；先核RequestId，绝不清理另一代请求的Timer。 */
    bool ExecuteDeferredRespawn(FString PlayerId, FGuid ExpectedRequestId);
    /** 出生每个外部调用边界复核本装配/阶段/原连接及Pawn；初次Countdown允许尚无已出生代次。 */
    bool IsSpawnScopeCurrent(const FString& PlayerId, TWeakObjectPtr<AGamePlatformArenaPlayerController> Controller,
        TWeakObjectPtr<AGamePlatformArenaPlayerState> State, TWeakObjectPtr<APawn> Pawn, FString MatchId,
        FString ServerId, FGuid BindingId, EGamePlatformArenaMatchPhase Phase, int32 Generation) const;

    void ClearRespawnTimers();

    TWeakObjectPtr<AGamePlatformArenaGameMode> GameMode;
    /** 借用装配租约，不由Adapter释放；装配销毁顺序保证Pawn先EndPlay。 */
    TArray<FGamePlatformDataLease> DefinitionWarmupLeases;
    TMap<FString, TWeakObjectPtr<APawn>> SpawnedPawns;
    TMap<FString, int32> SpawnGenerations;
    TMap<FString, FRespawnRequest> RespawnTimers;
};

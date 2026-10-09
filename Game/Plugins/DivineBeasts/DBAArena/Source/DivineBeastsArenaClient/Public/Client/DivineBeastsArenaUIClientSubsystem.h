#pragma once

#include "Arena/GamePlatformArenaTypes.h"
#include "Client/GamePlatformArenaClientTypes.h"
#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "DivineBeastsArenaUIClientSubsystem.generated.h"

class AGamePlatformArenaGameState;
class AGamePlatformArenaPlayerState;
class UGamePlatformArenaViewModel;
class UGamePlatformUIManagerSubsystem;
class UGamePlatformUIScreenDefinition;

/**
 * UDivineBeastsArenaUIClientSubsystem（神兽联盟竞技UI本地玩家子系统）。
 *
 * 作用域：
 * - 每个 LocalPlayer（本地玩家）独立实例，避免分屏/多PIE共享竞技UI状态。
 *
 * 职责：
 * 1. 注册 DBAArena 专属竞技 Screen Definition（页面定义），不污染公共 DBAClient。
 * 2. 持有并暴露 MobaCommon 的 UGamePlatformArenaViewModel（竞技只读视图模型）。
 * 3. 订阅 Arena GameState/PlayerState 的复制事件，并事件驱动刷新 ViewModel。
 * 4. 根据通用竞技 FlowState（流程状态）给出项目层稳定 UI SurfaceId（界面身份）。
 *
 * 性能约束：
 * - 不使用 Tick。
 * - 玩家统计只在复制事件到达时刷新；最多1v1～5v5共10名玩家，低频重绑成本有界。
 * - 不复制第二份比分/队伍状态模型，直接复用 MobaCommon ViewModel。
 */
UCLASS()
class DIVINEBEASTSARENACLIENT_API UDivineBeastsArenaUIClientSubsystem
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    virtual void PlayerControllerChanged(APlayerController* NewPlayerController) override;

    /** 返回当前本地玩家唯一的MOBA竞技只读ViewModel。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Arena|UI")
    UGamePlatformArenaViewModel* GetArenaViewModel() const
    {
        return ArenaViewModel;
    }

    /**
     * 从当前世界的竞技复制事实刷新绑定与ViewModel。
     * 由PlayerController切换、Arena复制事件或明确的世界进入事件触发，不应逐帧调用。
     */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Arena|UI")
    bool RefreshArenaViewFromWorld();

    /** 根据当前竞技流程状态返回应显示的主要竞技表面；无主要页面时返回None。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|Arena|UI")
    FName ResolvePrimaryArenaSurfaceId() const;

    /** 写入匹配/传输适配器观测到的非权威客户端流程状态。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|Arena|UI")
    void SetObservedArenaClientFlowState(
        EGamePlatformArenaClientFlowState InFlowState);

private:
    /** 当前LocalPlayer世界的GameState可迟到，订阅引擎设置事件；退出解绑并清空投影，无Tick重试。 */
    void BindWorldReadinessEvents(UWorld* World);
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    void RegisterArenaScreenDefinitions();
    void UnregisterArenaScreenDefinitions();

    /** 竞技GameState改变时重新绑定其公开复制事件。 */
    void EnsureGameStateBinding(AGamePlatformArenaGameState* GameState);

    /** 玩家列表发生变化时更新PlayerState统计事件绑定。 */
    void EnsurePlayerStateBindings(
        const TArray<AGamePlatformArenaPlayerState*>& PlayerStates);

    /** 清理GameState和PlayerState原生委托。 */
    void UnbindArenaEvents();

    /** 使用当前已绑定复制对象更新通用ArenaViewModel。 */
    bool RefreshViewModelFromBoundState();

    void HandleArenaPhaseChanged(
        EGamePlatformArenaMatchPhase MatchPhase,
        int32 Revision);
    void HandleArenaTeamStatesChanged(
        const TArray<FGamePlatformArenaTeamState>& TeamStates);
    void HandleArenaResultChanged(
        const FGamePlatformArenaResultSummary& Result);
    void HandleArenaPlayerStatsChanged(
        AGamePlatformArenaPlayerState* PlayerState);

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformUIManagerSubsystem> PlatformUI = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UGamePlatformArenaViewModel> ArenaViewModel = nullptr;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UGamePlatformUIScreenDefinition>> RegisteredDefinitions;

    TWeakObjectPtr<UWorld> BoundWorld;
    TWeakObjectPtr<UWorld> RetiredWorld;
    bool bDeinitializing = false;
    FDelegateHandle GameStateSetHandle;
    FDelegateHandle WorldCleanupHandle;
    TWeakObjectPtr<AGamePlatformArenaGameState> BoundGameState;
    TArray<TWeakObjectPtr<AGamePlatformArenaPlayerState>> BoundPlayerStates;
};

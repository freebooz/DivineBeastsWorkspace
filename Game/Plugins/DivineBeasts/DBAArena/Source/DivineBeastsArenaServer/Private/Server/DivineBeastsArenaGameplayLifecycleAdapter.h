#pragma once
#include "Engine/TimerHandle.h" // 私有适配器持有真实计时器句柄，不依赖调用文件隐式包含。

#include "CoreMinimal.h"
#include "Arena/GamePlatformArenaPolicies.h"

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
 */
class FDivineBeastsArenaGameplayLifecycleAdapter final
    : public IGamePlatformArenaGameplayLifecycleAdapter
    , public TSharedFromThis<FDivineBeastsArenaGameplayLifecycleAdapter>
{
public:
    explicit FDivineBeastsArenaGameplayLifecycleAdapter(
        AGamePlatformArenaGameMode& InGameMode);
    virtual ~FDivineBeastsArenaGameplayLifecycleAdapter() override;

    virtual bool SpawnPlayer(
        const FString& PlayerId,
        FName TeamId,
        FName SpawnPolicyId,
        FString& OutReason) override;

    virtual bool RequestRespawn(
        const FString& PlayerId,
        FName TeamId,
        FName RespawnPolicyId,
        float DelaySeconds,
        FString& OutReason) override;

private:
    bool ResolvePlayer(
        const FString& PlayerId,
        AGamePlatformArenaPlayerController*& OutController,
        AGamePlatformArenaPlayerState*& OutPlayerState,
        FString& OutReason) const;

    void ExecuteDeferredRespawn(
        FString PlayerId,
        FName TeamId,
        FName RespawnPolicyId);

    void ClearRespawnTimers();

    TWeakObjectPtr<AGamePlatformArenaGameMode> GameMode;
    TMap<FString, int32> SpawnGenerations;
    TMap<FString, FTimerHandle> RespawnTimers;
};

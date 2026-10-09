#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "Containers/Ticker.h"
#include "GamePlatformLocalHitstopSubsystem.generated.h"

class USkeletalMeshComponent;
class UWorld;

/**
 * UGamePlatformLocalHitstopSubsystem（本地命中视觉顿帧子系统）。
 *
 * 端侧：ClientOnly；作用域：LocalPlayer及其当前World。
 * 只暂停显式指定的骨骼网格动画，不修改World TimeDilation、GAS、
 * 角色移动组件、真实根运动结算、碰撞或服务器状态。
 * 当镜头/动画资源不可用时直接返回失败，不改变Gameplay结果。
 */
UCLASS()
class GAMEPLATFORMANIMATIONCLIENT_API UGamePlatformLocalHitstopSubsystem : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * 以60fps参考帧配置本次视觉顿帧。
     * @param EventId 可信命中事实标识；同一World内重复标识不二次触发。
     * @param SourceMesh 攻击方表现网格，可为空。
     * @param TargetMesh 受击方表现网格，可为空。
     * @param Frames 0..10帧，传0表示无需暂停。
     * @return 至少一个当前世界网格成功受理；不会返回服务器命中成功与否。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Combat Feedback")
    bool ApplyVisualHitstop(
        const FGuid& EventId,
        USkeletalMeshComponent* SourceMesh,
        USkeletalMeshComponent* TargetMesh,
        int32 Frames);

    /** 仅查询本LocalPlayer持有的视觉暂停，不代表玩法硬直。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Combat Feedback")
    bool IsVisualHitstopActive(const USkeletalMeshComponent* Mesh) const;

    /** 账户切换、世界退出或显式重置时恢复暂停过的组件并注销局部Ticker。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Combat Feedback")
    void CancelAllVisualHitstops();

private:
    /** 每个Mesh只保留一个到期时钟；覆盖为更长剩余时间，不进行加法叠加。 */
    struct FPausedMeshRecord
    {
        TWeakObjectPtr<USkeletalMeshComponent> Mesh;
        TWeakObjectPtr<UWorld> World;
        /** 基于单调实时时钟，不受世界TimeDilation变化影响。 */
        double DeadlineSeconds = 0.0;
        bool bWasAnimsPaused = false;
    };

    void ApplyToMesh(USkeletalMeshComponent* Mesh, UWorld& World, double DeadlineSeconds);
    void RestoreMesh(TWeakObjectPtr<USkeletalMeshComponent> Mesh);
    /** 只有存在活动视觉顿帧才临时注册Ticker，无永久逐帧更新。 */
    bool TickVisualHitstop(float DeltaSeconds);
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

    TMap<TWeakObjectPtr<USkeletalMeshComponent>, FPausedMeshRecord> ActiveMeshes;
    TSet<FGuid> RecentEventIds;
    TArray<FGuid> RecentEventOrder;
    TWeakObjectPtr<UWorld> BoundWorld;
    FDelegateHandle WorldCleanupHandle;
    FDelegateHandle ActiveTickHandle;

    static constexpr int32 MaxRecentEvents = 256;
    static constexpr int32 MaxVisualFrames = 10;
    static constexpr double ReferenceFps = 60.0;
};

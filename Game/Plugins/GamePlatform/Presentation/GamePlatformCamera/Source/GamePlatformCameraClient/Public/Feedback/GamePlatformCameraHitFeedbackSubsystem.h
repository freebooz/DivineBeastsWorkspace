#pragma once

#include "CoreMinimal.h"
#include "Subsystems/LocalPlayerSubsystem.h"
#include "GamePlatformCameraHitFeedbackSubsystem.generated.h"

class UCameraShakeBase;
class UWorld;

/**
 * UGamePlatformCameraHitFeedbackSubsystem（平台本地玩家命中镜头反馈）。
 * 仅操作当前LocalPlayer摄像机，UI/UMG画布不随CameraShake震动；
 * 不改角色位置、服务器权威瞄准、网络控制状态或World TimeDilation。
 */
UCLASS()
class GAMEPLATFORMCAMERACLIENT_API UGamePlatformCameraHitFeedbackSubsystem
    : public ULocalPlayerSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * 播放已预加载的CameraShake资产；不在命中关键路径同步加载。
     * EventId用于同世界重复通知去重；Strength在0..2内裁剪。
     */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Combat Feedback")
    bool PlayHitCameraShake(
        const FGuid& EventId,
        TSubclassOf<UCameraShakeBase> ShakeClass,
        float Strength);

    /** 玩家镜头舒适度倍率，0完全关闭。不能改变权威Gameplay。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Combat Feedback")
    void SetUserCameraShakeScale(float InScale);

    UFUNCTION(BlueprintPure, Category="GamePlatform|Combat Feedback")
    float GetUserCameraShakeScale() const { return UserCameraShakeScale; }

    /** 切图/账号切换可清除本地短期去重窗口，不停止其它系统持有的CameraShake。 */
    void ClearFeedbackHistory();

private:
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);

    /** 只缓存单个LocalPlayer最近256个事件身份，不保留强World指针。 */
    TSet<FGuid> RecentEventIds;
    TArray<FGuid> RecentEventOrder;
    TWeakObjectPtr<UWorld> BoundWorld;
    FDelegateHandle WorldCleanupHandle;
    double LastStartedAtSeconds = 0.0;
    float UserCameraShakeScale = 1.0f;

    static constexpr int32 MaxRecentEvents = 256;
};

// 天气客户端世界表现：Surface长期材质状态、Presentation短时特效请求共享权威快照。
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "GamePlatformWeatherClientWorldSubsystem.generated.h"

/** 当前客户端本世界天气插值结果通知；表现提供者不可用不影响服务器天气事实。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformWeatherVisualChanged, const FGamePlatformWeatherState&);

/** 在天气过渡期间才短期定时更新材质，完成后释放Timer；不复制服务器调度器。 */
UCLASS()
class GAMEPLATFORMWEATHERCLIENT_API UGamePlatformWeatherClientWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void OnWorldBeginPlay(UWorld& InWorld) override;
    virtual void Deinitialize() override;

    /** 只读当前已插值视觉天气；不允许用本值决定Gameplay伤害/导航或移动效果。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Weather")
    FGamePlatformWeatherState GetVisualWeather() const { return LastVisualState; }

    FDelegateHandle AddVisualChangedHandler(FGamePlatformWeatherVisualChanged::FDelegate Handler);
    void RemoveVisualChangedHandler(FDelegateHandle Handle);

private:
    void OnSnapshotReceived(const FGamePlatformWeatherSnapshot& Snapshot);
    void UpdateTransition();
    void ApplyVisualState(const FGamePlatformWeatherState& State);
    float GetEstimatedServerTimeSeconds() const;
    void SendPresentationEvent(const FGamePlatformWeatherState& State, bool bCancellation);
    void CancelPreviousPresentation();

    FGamePlatformWeatherSnapshot ActiveSnapshot;
    FGamePlatformWeatherState LastVisualState;
    FGamePlatformWeatherVisualChanged VisualChanged;
    FDelegateHandle SourceSnapshotHandle;
    FTimerHandle TransitionTimer;
    FGuid ActiveRequestId;
    FName ActiveVfxTag;
    FName ActiveSfxTag;
    bool bClosing = false;
};

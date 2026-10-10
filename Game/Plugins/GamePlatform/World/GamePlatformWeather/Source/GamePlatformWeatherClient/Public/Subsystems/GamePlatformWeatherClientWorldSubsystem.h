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

    /**
     * 项目内容包真实预载并发布目录后重发当前天气表现。
     * 只在本地世界消费既有权威快照，不创建新天气、不重复注册Provider。
     * 不对普通客户端提供天气权威写入。
     */
    void RefreshPresentationAfterContentActivation(bool bVisual);

private:
    void OnSnapshotReceived(const FGamePlatformWeatherSnapshot& Snapshot);
    void UpdateTransition();
    /** 分离快照首帧与Timer推进；避免首帧用未来目标天气瞬间播放满量雨雪。 */
    void SampleAndApplyTransition(bool bRefreshPresentation);
    void ApplyVisualState(const FGamePlatformWeatherState& State);
    float GetEstimatedServerTimeSeconds() const;
    /** ChannelFilter: -1全部，0仅VFX，1仅SFX；保持独立请求身份，禁止跨通道误取消。 */
    void SendPresentationEvent(const FGamePlatformWeatherState& State,
        bool bCancellation, int32 ChannelFilter = -1);
    void CancelPreviousPresentation(int32 ChannelFilter = -1);

    FGamePlatformWeatherSnapshot ActiveSnapshot;
    FGamePlatformWeatherState LastVisualState;
    FGamePlatformWeatherVisualChanged VisualChanged;
    FDelegateHandle SourceSnapshotHandle;
    FTimerHandle TransitionTimer;
    /** VFX和SFX使用不同请求身份，避免统一表现总线/播放器将两条请求去重成一条。 */
    FGuid ActiveVfxRequestId;
    FGuid ActiveSfxRequestId;
    FName ActiveVfxTag;
    FName ActiveSfxTag;
    /** 已提交的VFX/SFX天气强度；仅在过渡节点或完成时有限次重新发布，不按0.1s重建粒子。 */
    float LastPresentedIntensity = -1.f;
    bool bClosing = false;
};

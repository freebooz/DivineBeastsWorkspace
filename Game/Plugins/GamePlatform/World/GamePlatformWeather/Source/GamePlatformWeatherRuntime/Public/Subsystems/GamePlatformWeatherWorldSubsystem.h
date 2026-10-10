// 天气世界权威服务；一个世界作用域，不缓存账号/跨世界Actor，不轮询Tick。
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "Types/GamePlatformWeatherTypes.h"
#include "GamePlatformWeatherWorldSubsystem.generated.h"

class AGamePlatformWeatherReplicator;
class UGamePlatformWeatherPresetDefinition;

/** 网络天气快照本地通知；非网络事件，真实网络传输由复制Actor负责。 */
DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformWeatherSnapshotChanged, const FGamePlatformWeatherSnapshot&);

/** 平台天气调度入口；服务器唯一可写，客户端仅订阅快照并读取当前值。 */
UCLASS()
class GAMEPLATFORMWEATHERRUNTIME_API UGamePlatformWeatherWorldSubsystem final : public UWorldSubsystem
{
    GENERATED_BODY()
public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** 在可信世界GameMode已启用后调用；首次创建单个复制Actor，默认不自动启动随机天气。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Weather")
    bool ActivateWeather(const FGamePlatformWeatherState& InitialState);

    /** 仅服务器可手动变更；过渡秒数范围0..3600，自动调度会被取消。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Weather")
    bool SetWeather(const FGamePlatformWeatherState& TargetState, float TransitionSeconds);

    /** 采用外部GamePlatformData有效租约加载的资产，不接管对象生命周期。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Weather")
    bool ApplyPreset(const UGamePlatformWeatherPresetDefinition* Preset);

    /** 一次性验证并缓存完整调度表；seed只影响服务端选择，无客户端随机权威。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Weather")
    bool ConfigureSchedule(const TArray<FGamePlatformWeatherScheduleEntry>& Entries, int32 RandomSeed);

    /** 有效调度表及天气已激活时启动自动切换；每轮只注册一次下一轮定时器。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Weather")
    bool StartSchedule();
    UFUNCTION(BlueprintCallable, Category="GamePlatform|Weather")
    void StopSchedule();

    /** 迟加入者可直接读取最新网络快照；Revision=0表示尚无事实。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|Weather")
    FGamePlatformWeatherSnapshot GetCurrentSnapshot() const { return CurrentSnapshot; }

    /** 非蓝图C++订阅；调用方必须在世界或自身退出时解除绑定。 */
    FDelegateHandle AddSnapshotHandler(FGamePlatformWeatherSnapshotChanged::FDelegate Handler);
    void RemoveSnapshotHandler(FDelegateHandle Handle);

    /** 仅供本世界的复制Actor报告新快照；乱序、旧Actor和关闭世界数据被忽略。 */
    void ObserveReplicator(const AGamePlatformWeatherReplicator* Source, const FGamePlatformWeatherSnapshot& Snapshot);
    void ForgetReplicator(const AGamePlatformWeatherReplicator* Source);

private:
    /** 客户端不创建权威状态；真实Actor存在并且网络模式有权威资格才可提交。 */
    bool IsAuthoritativeWeatherWorld() const;
    bool PublishTransition(const FGamePlatformWeatherState& TargetState, float TransitionSeconds);
    void RunNextSchedule();

    UPROPERTY(Transient)
    TWeakObjectPtr<AGamePlatformWeatherReplicator> Replicator;
    FGamePlatformWeatherSnapshot CurrentSnapshot;
    FGamePlatformWeatherSnapshotChanged SnapshotChanged;
    TArray<FGamePlatformWeatherScheduleEntry> Schedule;
    FRandomStream Random;
    FTimerHandle ScheduleTimer;
    bool bScheduleRunning = false;
    bool bClosing = false;
};

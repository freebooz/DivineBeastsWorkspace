// 神兽联盟项目生态鸟群批次生成组件：复用正式Presentation/Data/VFX与Owner/Token，不创建第二套播放器。
// 单个定时器服务最多16个群组请求，无每鸟Actor/定时器/碰撞/复制；专用服务器不装载本Client模块。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Environment/DivineBeastsBirdFlockFlightSettings.h"
#include "TimerManager.h"
#include "DivineBeastsBirdFlockBatchComponent.generated.h"

class USplineComponent;
class AActor;
class UDivineBeastsPresentationClientSubsystem;

/**
 * UDivineBeastsBirdFlockBatchComponent（分批生成鸟群控制组件）。
 * 配置源在关卡项目层，服务请求经原有StartAmbientBirdFlockForOwnerAdvanced提交；
 * 所有已受理群组的唯一注册Token由本组件掌管，停止仅回收本Owner的请求。
 */
UCLASS(ClassGroup=(DivineBeasts),meta=(BlueprintSpawnableComponent))
class DIVINEBEASTSPRESENTATIONCLIENT_API UDivineBeastsBirdFlockBatchComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UDivineBeastsBirdFlockBatchComponent();

    /** 不在未知本地玩家就绪阶段默认启动；场景蓝图应在客户端Ready通知中显式开始。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Spawner")
    bool bAutoStart = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Spawner")
    FName GroupPrefix = TEXT("Ambient.BirdSpline");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Spawner",
        meta=(GetOptions="GetAvailableSpecies"))
    FName SpeciesId = TEXT("Swallow");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Spawner", meta=(ClampMin="0",ClampMax="512"))
    int32 BirdsPerGroup = 32;

    /** 本Owner最多活跃1..16群，平台层真实运行数量仍受EffectType预算裁剪。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Spawner", meta=(ClampMin="1",ClampMax="16"))
    int32 MaxActiveGroups = 3;

    /** 总共受理的生成次数，0表示持续生成直至组件或世界显式停止。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Spawner", meta=(ClampMin="0",ClampMax="10000"))
    int32 MaxBatches = 4;

    /** 生成间隔秒数，低于0.25拒绝；对每批使用同一个调度器，不逐鸟Tick。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Spawner", meta=(ClampMin="0.25",ClampMax="600",Units="s"))
    float SpawnIntervalSeconds = 6.f;

    /** 起点局部扰动范围厘米，避免整群重叠；受Flight安全空间约束。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Spawner")
    FVector SpawnHalfExtentsCm = FVector(150.f,150.f,80.f);

    /** 允许路线Actor持有一个已注册Spline，空则读取本Owner上的组件。 */
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category="BirdFlock|Route")
    TObjectPtr<AActor> RouteActor = nullptr;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Route")
    FDivineBeastsBirdFlockFlightSettings Flight;

    /** 标注路径单次飞行的真实巡航速度，以厘米/秒计算有限生命周期。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="BirdFlock|Route", meta=(ClampMin="10",ClampMax="5000",Units="cm/s"))
    float CruiseSpeedCmPerSecond = 250.f;

    /** 人工启动；成功仅指进入可生成状态，Niagara资源受理状态依真实服务回执。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|BirdFlock|Spawner")
    bool StartBatches(FString& OutError);

    /** 只撤销当前Owner所有已受理Token；重复调用安全，不清除其他场景鸟群。 */
    UFUNCTION(BlueprintCallable, Category="DivineBeasts|BirdFlock|Spawner")
    void StopBatches();

    /** 查询当前本组件持有群组请求数量，不等于Niagara GPU实际可见粒子数。 */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|BirdFlock|Spawner")
    int32 GetOwnedGroupCount() const { return Groups.Num(); }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|BirdFlock|Spawner")
    FString GetLastError() const { return LastError; }

    UFUNCTION(BlueprintPure, Category="DivineBeasts|BirdFlock|Spawner")
    TArray<FString> GetAvailableSpecies() const;

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    struct FActiveGroup
    {
        FName GroupId;
        FGuid RegistrationToken;
        /** 单程样条在终点到达后自动撤销，0表示循环/往返由轮换或关闭回收。 */
        double ExpireAtWorldSeconds = 0.0;
    };
    TArray<FActiveGroup> Groups;
    TWeakObjectPtr<UDivineBeastsPresentationClientSubsystem> Service;
    FTimerHandle ScheduleHandle;
    FRandomStream Random;
    FString LastError;
    int32 SpawnedBatches = 0;
    double NextSpawnWorldSeconds = 0.0;
    bool bActive = false;
    bool bEndingPlay = false;

    void ScheduleStep();
    bool SpawnOneGroup(FString& OutError);
    void ReleaseGroup(const FActiveGroup& Group);
};

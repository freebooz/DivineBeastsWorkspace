// UGamePlatformWaterInteractorComponent（跨游戏水域交互权威组件）：服务端低频采样并复制离散水域状态。
// 不依赖项目英雄、VFX、Flow、Niagara或客户端画面；正式角色可在项目层有选择地装配。
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GamePlatformWaterInteractorComponent.generated.h"

/** FGamePlatformWaterPresence（水域权威状态）只同步过渡，不每帧传输水深/波纹。
 * SurfaceZ和实时流速是区域静态数据，客户端可通过相同地图内容按需读取。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMWATERRUNTIME_API FGamePlatformWaterPresence
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Water")
    bool bInWater = false;

    UPROPERTY(BlueprintReadOnly, Category="Water")
    bool bDeepWater = false;

    UPROPERTY(BlueprintReadOnly, Category="Water")
    bool bCanSwim = false;

    UPROPERTY(BlueprintReadOnly, Category="Water")
    FName RegionId = NAME_None;

    bool HasSameState(const FGamePlatformWaterPresence& Other) const
    {
        return bInWater == Other.bInWater && bDeepWater == Other.bDeepWater &&
            bCanSwim == Other.bCanSwim && RegionId == Other.RegionId;
    }
};

/** FGamePlatformWaterPresenceChanged（水域状态事件）：不保证远端同一帧发生。 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
    FGamePlatformWaterPresenceChanged, const FGamePlatformWaterPresence&, State);

/** C++中立水域过渡事件；只通过内存委托传送，不成为第二条网络状态通道。 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FGamePlatformWaterPresenceChangedNative,
    const AActor*, const FGamePlatformWaterPresence&);

/** 权威水域体积变化事件，无客户端输入权限；角色移动性能不依赖WaterBody材质。 */
UCLASS(ClassGroup=(GamePlatform), BlueprintType, meta=(BlueprintSpawnableComponent))
class GAMEPLATFORMWATERRUNTIME_API UGamePlatformWaterInteractorComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UGamePlatformWaterInteractorComponent();

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** 只有角色服务器权威Tick计时器采样，范围0.1~2秒，非Tick轮询；低配可调整。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Water|Authority", meta=(ClampMin="0.1", ClampMax="2.0"))
    float QueryIntervalSeconds = 0.3f;

    /** 脚部相对角色Actor位置的垂直偏移（cm），默认-85cm；无需每帧射线查地形。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Water|Authority", meta=(ClampMin="-300", ClampMax="300"))
    float FootSampleOffsetCm = -85.f;

    /** 只读状态由服务器通过地图水域索引生成，并在离散改变时网络复制。 */
    UPROPERTY(ReplicatedUsing=OnRep_WaterState, BlueprintReadOnly, Category="Water|Authority")
    FGamePlatformWaterPresence WaterState;

    /** 状态改变时由GameThread发出；本地特效只能读取，禁止修改权威状态。 */
    UPROPERTY(BlueprintAssignable, Category="Water|Authority")
    FGamePlatformWaterPresenceChanged OnWaterStateChanged;
    /** 客户端视觉层只监听原生委托；组件本身不依赖Niagara和项目素材。 */
    FGamePlatformWaterPresenceChangedNative OnWaterStateChangedNative;

    UFUNCTION(BlueprintPure, Category="GamePlatform|Water")
    FGamePlatformWaterPresence GetWaterState() const { return WaterState; }

private:
    UFUNCTION()
    void OnRep_WaterState();

    /** 定时器回调只在有权威的Actor执行，客户端不运行另一套决策。 */
    void PollAuthorityWater();
    void ResetAuthorityWater();

    FTimerHandle QueryTimer;
};

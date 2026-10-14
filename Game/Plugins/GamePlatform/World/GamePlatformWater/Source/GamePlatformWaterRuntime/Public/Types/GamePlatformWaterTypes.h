// GamePlatformWaterRuntime（平台层双端水域逻辑）只存储可供服务器查询的几何与规则。
// 不包含Water、Niagara、FluidFlux、材质及任意客户端渲染或游戏项目命名。
#pragma once

#include "CoreMinimal.h"
#include "GamePlatformWaterTypes.generated.h"

/** EGamePlatformWaterKind（水域类型）：只供权威玩法查询与客户端外观映射。 */
UENUM(BlueprintType)
enum class EGamePlatformWaterKind : uint8
{
    Lake UMETA(DisplayName="湖泊"),
    River UMETA(DisplayName="河流"),
    Stream UMETA(DisplayName="溪流"),
    Pool UMETA(DisplayName="水潭"),
    Shore UMETA(DisplayName="浅水岸线")
};

/** FGamePlatformWaterRegionConfig（静态水域权威配置），单位以cm、cm/s为准。
 * 由地图设计与服务器共同加载，不接受普通客户端网络RPC修改。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMWATERRUNTIME_API FGamePlatformWaterRegionConfig
{
    GENERATED_BODY()

    /** RegionId（水域稳定身份），同一World内不允许重复，配置修改需要人工审核。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water|Authority")
    FName RegionId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water|Authority")
    EGamePlatformWaterKind Kind = EGamePlatformWaterKind::Lake;

    /** Priority（重叠时选择高优先级区域），合法区间0~100。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water|Authority", meta=(ClampMin="0", ClampMax="100"))
    int32 Priority = 20;

    /** SurfaceOffsetCm（水面相对Actor原点垂直偏移），可负值，绝对值限制100000厘米。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water|Authority", meta=(ClampMin="-100000", ClampMax="100000"))
    float SurfaceOffsetCm = 0.f;

    /** ShallowDepthCm（浅水阈值），范围1~1000厘米。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water|Authority", meta=(ClampMin="1", ClampMax="1000"))
    float ShallowDepthCm = 90.f;

    /** FlowDirection（水流水平向量），零向量视为静水；非零会归一化。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water|Authority")
    FVector2D FlowDirection = FVector2D(1.0, 0.0);

    /** FlowSpeedCmPerSecond（权威平均水平流速），0~1500厘米/秒。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water|Authority", meta=(ClampMin="0", ClampMax="1500"))
    float FlowSpeedCmPerSecond = 0.f;

    /** 是否允许游泳；浅水地形或危险水池可禁用。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water|Authority")
    bool bSwimmable = true;

    /** 是否允许客户端按当前水域播放本地接触水花；不产生服务器伤害。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Water|Presentation")
    bool bAllowLocalRipples = true;

    bool IsValid() const
    {
        return !RegionId.IsNone() && Priority >= 0 && Priority <= 100 &&
            FMath::IsFinite(SurfaceOffsetCm) && FMath::Abs(SurfaceOffsetCm) <= 100000.f &&
            FMath::IsFinite(ShallowDepthCm) && ShallowDepthCm >= 1.f && ShallowDepthCm <= 1000.f &&
            FMath::IsFinite(FlowDirection.X) && FMath::IsFinite(FlowDirection.Y) &&
            FMath::IsFinite(FlowSpeedCmPerSecond) &&
            FlowSpeedCmPerSecond >= 0.f && FlowSpeedCmPerSecond <= 1500.f;
    }
};

/** FGamePlatformWaterSample（水域局部查询结果）：bInWater为真才允许将其他字段用作玩法输入。
 * 由地图作者保证碰撞底界/水面高度语义与服务器世界地图一致。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMWATERRUNTIME_API FGamePlatformWaterSample
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Water")
    bool bInWater = false;
    UPROPERTY(BlueprintReadOnly, Category="Water")
    FName RegionId = NAME_None;
    UPROPERTY(BlueprintReadOnly, Category="Water")
    EGamePlatformWaterKind Kind = EGamePlatformWaterKind::Lake;
    UPROPERTY(BlueprintReadOnly, Category="Water")
    float SurfaceZCm = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Water")
    float DepthCm = 0.f;
    UPROPERTY(BlueprintReadOnly, Category="Water")
    bool bShallow = false;
    UPROPERTY(BlueprintReadOnly, Category="Water")
    bool bSwimmable = false;
    UPROPERTY(BlueprintReadOnly, Category="Water")
    bool bAllowLocalRipples = false;
    UPROPERTY(BlueprintReadOnly, Category="Water")
    FVector FlowVelocityCmPerSecond = FVector::ZeroVector;
};

/** FGamePlatformWaterMath（纯水域数学），无世界/网络/贴图依赖，测试可独立执行。 */
struct GAMEPLATFORMWATERRUNTIME_API FGamePlatformWaterMath
{
    /** 查询Box水平范围、垂直深度和权威流速；失败时OutSample清空，永不返回旧世界数据。 */
    static bool SampleWaterVolume(
        const FTransform& BoxWorldTransform,
        const FVector& UnscaledBoxHalfExtent,
        float SurfaceZCm,
        const FVector& WorldPosition,
        const FGamePlatformWaterRegionConfig& Config,
        FGamePlatformWaterSample& OutSample);
};

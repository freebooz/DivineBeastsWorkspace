// GamePlatform平台层：鸟群客户端表现通用配置，不定义任何具体游戏鸟类、技能或服务器权威数据。
// 配置是纯客户端DataAsset；项目通过独立内容包提供鸟模型/材质实例，不能放入Dedicated Server内容。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GamePlatformBirdFlockProfile.generated.h"

class UStaticMesh;
class UMaterialInterface;

/** 鸟群飞行模式，只决定可视实例的轨迹，不承载玩法或导航决策。 */
UENUM(BlueprintType)
enum class EGamePlatformBirdFlockFlightMode : uint8
{
    Orbit UMETA(DisplayName="环绕盘旋"),
    Traverse UMETA(DisplayName="定向掠过"),
    FigureEight UMETA(DisplayName="八字往复")
};

/**
 * 通用鸟群预设：指定物种的客户侧网格/材质引用、数量上限和运动参数。
 * 不持有种类枚举或神兽联盟专有语义；实例切换预设即切换物种。
 * 资源随客户端表现内容包加载，不能从服务器权威流程引用该DataAsset。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMVFXCLIENT_API UGamePlatformBirdFlockProfile final : public UDataAsset
{
    GENERATED_BODY()

public:
    /** 展示名称仅供策划识别，真正物种身份由项目内容资产路径/DefinitionId持有。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Identity")
    FName DisplaySpeciesId = NAME_None;

    /** 飞行鸟类网格：X轴机头朝前，顶点色RGBA中A为翼尖振翅权重。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Rendering")
    TObjectPtr<UStaticMesh> BirdMesh = nullptr;

    /** 材质支持GPU顶点振翅；只在客户端可视组件上覆盖网格材质槽。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Rendering")
    TObjectPtr<UMaterialInterface> BirdMaterial = nullptr;

    /** 默认实例数量；0代表关闭，不会产生零尺寸或隐形鸟群。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Count", meta=(ClampMin="0", ClampMax="512"))
    int32 DefaultBirdCount = 32;

    /** 该预设最大实例数量：不得超过硬上限512，按设备配置可进一步降低。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Count", meta=(ClampMin="0", ClampMax="512"))
    int32 MaxBirdCount = 128;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Motion")
    EGamePlatformBirdFlockFlightMode FlightMode = EGamePlatformBirdFlockFlightMode::Orbit;

    /** 局部空间飞行半径，单位cm，最小200以避免出现不稳定高角速度。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Motion", meta=(ClampMin="200", ClampMax="100000"))
    float RadiusCm = 1500.0f;

    /** 相对锚点的中心飞行高度，单位cm，不参与权威碰撞检测。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Motion", meta=(ClampMin="0", ClampMax="100000"))
    float FlightHeightCm = 800.0f;

    /** 巡航速度，单位cm/s，仅影响可视实例的轨迹。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Motion", meta=(ClampMin="10", ClampMax="5000"))
    float CruiseSpeedCmPerSecond = 240.0f;

    /** 鸟群内部的高度变化幅度，单位cm。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Motion", meta=(ClampMin="0", ClampMax="2000"))
    float HeightVariationCm = 135.0f;

    /** 相对风场漂移位移幅度，单位cm（不累计积分，防止无限偏移）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Motion", meta=(ClampMin="0", ClampMax="2000"))
    float WindInfluenceCm = 95.0f;

    /** 可视更新频率，单位Hz，不改变材质GPU振翅的帧内频率。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Performance", meta=(ClampMin="5", ClampMax="60"))
    float UpdateRateHz = 20.0f;

    /** 距离裁剪下限cm；0代表不裁剪近景。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Performance", meta=(ClampMin="0"))
    int32 MinDrawDistanceCm = 0;

    /** 距离裁剪上限cm，不能小于下限；仅本地实例渲染使用。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Performance", meta=(ClampMin="1000"))
    int32 MaxDrawDistanceCm = 25000;

    /** 验证静态配置是否可执行；错误时返回false及可供开发定位的原因。 */
    bool Validate(FString& OutReason) const;
};

/** 纯数学接口：零Actor、零网络、零粒子系统依赖，供运行时和自动化测试复用。 */
struct GAMEPLATFORMVFXCLIENT_API FGamePlatformBirdFlockMath
{
    static constexpr int32 HardMaxBirdCount = 512;

    /** CountOverride=-1仅用于选择预设默认值；错误值返回INDEX_NONE。 */
    static int32 ResolveBirdCount(const UGamePlatformBirdFlockProfile& Profile,
                                  int32 CountOverride, float QualityFraction);

    /** 稳定可复现的随机数[0,1)，避免在Tick中反复生成随机种子。 */
    static float SeedToUnit(int32 RandomSeed, int32 BirdIndex, uint32 Salt = 0);
};

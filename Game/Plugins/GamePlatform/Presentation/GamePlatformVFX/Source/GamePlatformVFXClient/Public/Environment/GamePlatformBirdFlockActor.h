// GamePlatform平台层：世界本地客户端鸟群可视Actor，负责同一网格批量实例的轨迹与生命周期。
// 非Gameplay实体；正式表现请求依然由GamePlatformPresentation和GamePlatformVFX服务承载。
// 该Actor仅供客户端环境布置及编辑器预览，禁止放入Dedicated Server使用的共享地图。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Environment/GamePlatformBirdFlockProfile.h"
#include "GamePlatformBirdFlockActor.generated.h"

class UInstancedStaticMeshComponent;
class USceneComponent;

/** 同一模型的鸟群实例使用单一ISM，不创建逐鸟Actor、独立骨骼或复制状态。 */
UCLASS(BlueprintType, Blueprintable)
// 允许项目蓝图从通用Actor派生，仅覆盖物种/数量配置，不重写飞行执行器。
class GAMEPLATFORMVFXCLIENT_API AGamePlatformBirdFlockActor : public AActor
{
    GENERATED_BODY()

public:
    AGamePlatformBirdFlockActor();

    virtual void OnConstruction(const FTransform& Transform) override;
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /** 主场景配置；实例只持有客户端项目ContentPack中的DataAsset引用。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Configuration")
    TObjectPtr<UGamePlatformBirdFlockProfile> BirdProfile = nullptr;

    /** -1使用预设默认数量；0关闭，正整数按预设MaxBirdCount与画质因子限制。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Configuration",
              meta=(ClampMin="-1", ClampMax="512"))
    int32 BirdCountOverride = -1;

    /** 预设的种子覆盖，仅改变位置/转弯/振翅相位，不用于玩法权威状态。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Configuration")
    int32 FlockSeed = 20261011;

    /** 质量乘数0..1；0不分配可视实例，不会保留不可见的鸟群。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Performance",
              meta=(ClampMin="0", ClampMax="1"))
    float QualityFraction = 1.0f;

    /** 本地风场方向，单位方向向量；零向量代表无风。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Motion")
    FVector WindDirection = FVector::ZeroVector;

    /** 编辑器放置后显示固定姿态预览；不会在编辑模式下启动Tick模拟。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="BirdFlock|Editor")
    bool bShowEditorPreview = true;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="BirdFlock|Rendering")
    TObjectPtr<UInstancedStaticMeshComponent> BirdInstances;

    /** 动态切换鸟类类型：传入项目层鸟类DataAsset，立即刷新当前可视实例。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|VFX|BirdFlock")
    bool SetBirdProfile(UGamePlatformBirdFlockProfile* NewProfile);

    /** 动态指定数量0..512；失败不改变旧请求值。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|VFX|BirdFlock")
    bool SetBirdCount(int32 NewCount);

    /** 动态指定质量比例0..1，低画质减少可见鸟数。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|VFX|BirdFlock")
    bool SetQualityFraction(float NewFraction);

    /** 更新局部风场方向；只影响外观，不更改服务器状态。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|VFX|BirdFlock")
    void SetWindDirection(const FVector& NewDirection);

    /** 按当前配置重建实例；也用于编辑器预览及异常配置显式回退。 */
    UFUNCTION(BlueprintCallable, Category="GamePlatform|VFX|BirdFlock")
    bool RefreshFlock();

    /** 返回真正渲染的实例数，不把配置请求数误报为实际数。 */
    UFUNCTION(BlueprintPure, Category="GamePlatform|VFX|BirdFlock")
    int32 GetEffectiveBirdCount() const;

private:
    /** 个体初始化后不随Tick变化的确定性参数；不会跨网络复制。 */
    struct FBirdSample
    {
        float HeadingRadians = 0.0f;
        float RadiusFactor = 1.0f;
        float HeightFactor = 0.0f;
        float SpeedFactor = 1.0f;
        float ScaleFactor = 1.0f;
    };

    UPROPERTY(VisibleAnywhere, Category="BirdFlock|Rendering")
    TObjectPtr<USceneComponent> Root;

    TArray<FBirdSample> Birds;
    float UpdateAccumulatorSeconds = 0.0f;
    double SimulationTimeSeconds = 0.0;

    bool CanRenderLocally() const;
    FVector SampleLocalPosition(const FBirdSample& Bird, double AtSeconds) const;
    void UpdateBirdTransforms(double AtSeconds);
    void ClearFlock();
};

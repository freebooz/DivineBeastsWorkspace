// 平台客户端水体表现预设：只读异步软资产目录与质量预算；可按游戏项目创建DataAsset实例。
// 所有GPU粒子是可选增强，无资源时不阻止服务器的水域判定、游泳或任务。
#pragma once

#include "CoreMinimal.h"
#include "Engine/PrimaryDataAsset.h"
#include "GamePlatformWaterPresentationPreset.generated.h"

class UNiagaraSystem;

/** UGamePlatformWaterPresentationPreset（客户端水体表现配置）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMWATERCLIENT_API UGamePlatformWaterPresentationPreset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** RippleSystem（局部涟漪Niagara），弱资源引用：仅客户端按需异步加载。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water|Presentation")
    TSoftObjectPtr<UNiagaraSystem> RippleSystem;

    /** SplashSystem（脚步飞溅Niagara），失败可只显示涟漪。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water|Presentation")
    TSoftObjectPtr<UNiagaraSystem> SplashSystem;

    /** MaxRippleSpawnsPerSecond（单客户端单世界每秒最大特效请求），范围0~96。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water|Scalability", meta=(ClampMin="0", ClampMax="96"))
    int32 MaxRippleSpawnsPerSecond = 18;

    /** MaxViewDistanceCm（最远可见距离），范围100~30000厘米。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water|Scalability", meta=(ClampMin="100", ClampMax="30000"))
    float MaxViewDistanceCm = 6000.f;

    /** MinimalTimeBetweenSamePointSeconds（相同水面位置重复冲击间隔），避免脚步事件风暴。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water|Scalability", meta=(ClampMin="0.01", ClampMax="1.0"))
    float MinimalTimeBetweenSamePointSeconds = 0.13f;

    /** bEnableSplash（是否启动飞溅）；低档可关闭，只保留水面涟漪。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water|Scalability")
    bool bEnableSplash = true;
};

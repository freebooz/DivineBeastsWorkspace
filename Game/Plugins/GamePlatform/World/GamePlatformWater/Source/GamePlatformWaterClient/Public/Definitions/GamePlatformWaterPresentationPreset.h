// 平台客户端水体表现预设只保存中立语义标签和预算，不直接引用或加载任何Niagara资源。
// 统一的VFX/SFX资源预载、内容解析、执行和取消由GamePlatformPresentation/GamePlatformVFX负责。
#pragma once

#include "CoreMinimal.h"
// UE5.8的UPrimaryDataAsset声明位于DataAsset.h；只修复真实引擎包含路径，不改变并行水体预设合同。
#include "Engine/DataAsset.h"
#include "GamePlatformWaterPresentationPreset.generated.h"

/** UGamePlatformWaterPresentationPreset（客户端水体表现配置）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMWATERCLIENT_API UGamePlatformWaterPresentationPreset : public UPrimaryDataAsset
{
    GENERATED_BODY()
public:
    /** RippleSemanticTag（平台表现涟漪语义）；Niagara实际Definition由现有表现目录解析。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water|Presentation")
    FName RippleSemanticTag = TEXT("Presentation.Water.Ripple.VFX");

    /** SplashSemanticTag（脚步/落水飞溅语义）；没有内容目录时允许降级只播涟漪。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Water|Presentation")
    FName SplashSemanticTag = TEXT("Presentation.Water.Splash.VFX");

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

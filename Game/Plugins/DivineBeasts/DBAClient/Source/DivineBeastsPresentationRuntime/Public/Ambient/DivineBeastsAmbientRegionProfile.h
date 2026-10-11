#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DivineBeastsAmbientRegionProfile.generated.h"

/**
 * FDivineBeastsAmbientStyleWeight（神兽联盟区域自然环境视觉风格权重）。
 * 仅用于客户端装饰表现选择，不携带碰撞、权威天气或战斗判定。
 */
USTRUCT(BlueprintType)
struct FDivineBeastsAmbientStyleWeight
{
    GENERATED_BODY()

    /** StyleId（外观身份），如PearPetal/Maple/Firefly，由项目Definition目录解析。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AmbientNature")
    FName StyleId = NAME_None;

    /** Weight（相对权重，非粒子生成率），必须为有限正数才参与选择。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AmbientNature", meta=(ClampMin="0.0"))
    float Weight = 1.f;
};

/**
 * UDivineBeastsAmbientRegionProfile（神兽联盟自然环境区域预设数据资产）。
 * 项目层负责区域语义、种类和权重；实际播放统一交给GamePlatformPresentation/VFX。
 * 不持有UNiagaraComponent或全局粒子池，不复制服务器任何视觉粒子。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSPRESENTATIONRUNTIME_API UDivineBeastsAmbientRegionProfile : public UDataAsset
{
    GENERATED_BODY()

public:
    /** AreaKey（区域身份），例如OpenWorld.MapleSlope；并非GameplayTag注册声明。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AmbientNature|Identity")
    FName AreaKey = NAME_None;

    /** LabelZh（中文场景备注，仅编辑器展示）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AmbientNature|Identity")
    FString LabelZh;

    /** SpawnHeightMinCm / MaxCm（相对于区域锚点的生成高度范围，厘米）。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AmbientNature|Placement", meta=(ClampMin="0.0"))
    float SpawnHeightMinCm = 200.f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AmbientNature|Placement", meta=(ClampMin="0.0"))
    float SpawnHeightMaxCm = 1000.f;

    /** WindStrength（客户端预设风量系数），运行时以已验证的天气合同为准。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AmbientNature|Placement", meta=(ClampMin="0.0"))
    float WindStrength = 0.4f;

    /** Styles（区域外观池），只包含权重与逻辑身份；资源预载成功才能播放。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="AmbientNature|Styles")
    TArray<FDivineBeastsAmbientStyleWeight> Styles;

    /**
     * SelectStyle（按0..1选择一种自然环境视觉风格）。
     * @param NormalizedSample 归一化可复现选择值，非加密随机数；超范围会裁剪。
     * @return 匹配风格身份；没有合法权重或无有效身份返回NAME_None。
     */
    UFUNCTION(BlueprintPure, Category="DivineBeasts|AmbientNature")
    FName SelectStyle(float NormalizedSample) const;
};

#pragma once

#include "CoreMinimal.h"
#include "Definitions/GamePlatformDefinitionBase.h"
class UCameraShakeBase;
class UMaterialInterface;
#include "GamePlatformHitFeedbackProfile.generated.h"

/**
 * 平台通用命中反馈参数，归 GamePlatformPresentationCore 所有。
 * 仅描述客户端表现强度，不决定服务器伤害、硬直、碰撞或真实击退。
 * 帧数以固定的每秒60帧换算成秒，不依赖渲染帧率。
 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMPRESENTATIONCORE_API FGamePlatformHitFeedbackTuning
{
    GENERATED_BODY()

    /** 轻击参考顿帧数；零表示关闭。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Hitstop", meta=(ClampMin="0", ClampMax="10"))
    int32 LightHitstopFrames = 3;

    /** 重击参考顿帧数；不得用于延长游戏性控制。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Hitstop", meta=(ClampMin="0", ClampMax="10"))
    int32 HeavyHitstopFrames = 6;

    /** 技能命中参考顿帧数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Hitstop", meta=(ClampMin="0", ClampMax="10"))
    int32 SkillHitstopFrames = 5;

    /** 格挡时的短促顿帧数。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Hitstop", meta=(ClampMin="0", ClampMax="10"))
    int32 BlockHitstopFrames = 1;

    /** 破防额外帧数；仅由可信规则显式指定。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Hitstop", meta=(ClampMin="0", ClampMax="3"))
    int32 GuardBreakExtraFrames = 2;

    /** 单次局部顿帧上限，默认10帧。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Hitstop", meta=(ClampMin="0", ClampMax="10"))
    int32 MaxHitstopFrames = 10;

    /** 命中闪白时长（秒）；闪白只作用于受击模型。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Visual", meta=(ClampMin="0.0", ClampMax="0.05"))
    float HitFlashSeconds = 0.025f;

    /** 镜头震动强度系数；玩家设置还可乘独立舒适度倍率。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Camera", meta=(ClampMin="0.0", ClampMax="2.0"))
    float CameraStrength = 1.0f;

    /** 命中音量系数，由 SFX 通道独立执行和限制并发。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Audio", meta=(ClampMin="0.0", ClampMax="2.0"))
    float AudioStrength = 1.0f;

    /** Niagara视觉密度系数，不作为粒子计数或生产预算。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|VFX", meta=(ClampMin="0.0", ClampMax="2.0"))
    float VFXStrength = 1.0f;

    /** 每增加一段有效连击的表现增强比例，不延长顿帧。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Combo", meta=(ClampMin="0.0", ClampMax="0.3"))
    float ComboStrengthPerStep = 0.075f;

    /** 连击增强上限，避免多段技能反馈无限放大。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Combo", meta=(ClampMin="1.0", ClampMax="2.0"))
    float MaxComboStrength = 1.45f;

    /** 格挡后的通道强度倍率；打空不产生接触反馈。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Combo", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BlockStrengthRatio = 0.35f;

    /** 本地玩家受击时的额外表现倍率；不影响受控时间。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback|Local Player", meta=(ClampMin="1.0", ClampMax="1.5"))
    float LocalVictimStrengthRatio = 1.15f;
};

/**
 * UGamePlatformHitFeedbackProfile（平台命中反馈资源）。
 * 可在编辑器建立DataAsset；游戏项目只引用资源，不在平台代码硬编码英雄或技能。
 * 本类是可配置契约，不自动加载或实例化任何Niagara、音频或UI资产。
 */
UCLASS(BlueprintType)
class GAMEPLATFORMPRESENTATIONCORE_API UGamePlatformHitFeedbackProfile : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()

public:
    /** 完整客户端命中反馈调校参数，支持为不同技能配置资源实例。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Hit Feedback")
    FGamePlatformHitFeedbackTuning Tuning;

    /** 已预加载的客户端镜头震动类；由CameraClient执行，可为空以完全跳过。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Feedback|Camera", meta=(AssetBundles="Client"))
    TSoftClassPtr<UCameraShakeBase> CameraShakeClass;

    /**
     * 通过GamePlatformData的客户端资产Bundle异步预加载的命中Overlay材质；由AnimationClient执行。
     * 专用服务器不实例化或Cook纯客户端表现资源。
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Hit Feedback|Visual", meta=(AssetBundles="Client"))
    TSoftObjectPtr<UMaterialInterface> HitFlashOverlayMaterial;

    /**
     * 继承GamePlatformData的稳定主资产定义校验。
     * 客户端表现数值必须有限且位于允许范围；资源可缺失以安全降级。
     */
    virtual FGamePlatformResult ValidateDefinition() const override;
};

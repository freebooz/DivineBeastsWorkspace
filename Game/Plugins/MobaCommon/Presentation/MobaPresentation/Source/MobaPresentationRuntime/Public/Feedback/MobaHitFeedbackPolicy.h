#pragma once

#include "CoreMinimal.h"
#include "Feedback/GamePlatformHitFeedbackProfile.h"
#include "MobaHitFeedbackPolicy.generated.h"

/**
 * MOBA通用命中接触类型。只表达玩法已确认的接触类别，不由伤害数字猜测攻击轻重。
 */
UENUM(BlueprintType)
enum class EMobaHitFeedbackContact : uint8
{
    Light,   // 轻击
    Heavy,   // 重击
    Skill,   // 技能接触
    Blocked, // 已确认格挡
    Missed   // 挥空或闪避，没有有效接触
};

/**
 * MOBA已确认命中反馈的中立输入。
 * 输入由战斗事实/技能定义填充；本结构不用于客户端判定命中或伤害。
 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaHitFeedbackInput
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback")
    EMobaHitFeedbackContact Contact = EMobaHitFeedbackContact::Light;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback")
    bool bCritical = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback")
    bool bGuardBroken = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback")
    bool bLocalVictim = false;

    /** 连击第一段为1；格挡与挥空不会累计连击强度。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Hit Feedback", meta=(ClampMin="1", ClampMax="32"))
    int32 ComboStep = 1;
};

/** 仅表现参数，不包含GAS控制、权威位移或真实伤害字段。 */
USTRUCT(BlueprintType)
struct MOBAPRESENTATIONRUNTIME_API FMobaHitFeedbackDecision
{
    GENERATED_BODY()

    /** 是否实际发生接触；false时不得触发命中特效/音效/顿帧。 */
    UPROPERTY(BlueprintReadOnly, Category="Hit Feedback")
    bool bHasContact = false;

    UPROPERTY(BlueprintReadOnly, Category="Hit Feedback")
    int32 VisualHitstopFrames = 0;

    /** 与刷新率无关的固定60Hz参考秒数。 */
    UPROPERTY(BlueprintReadOnly, Category="Hit Feedback")
    float VisualHitstopSeconds = 0.0f;

    /** 用于VFX、SFX与镜头等提供者的强度系数，不改变Gameplay属性。 */
    UPROPERTY(BlueprintReadOnly, Category="Hit Feedback")
    float Strength = 0.0f;

    /** 命中闪白持续秒数，不包含资源加载或材质控制逻辑。 */
    UPROPERTY(BlueprintReadOnly, Category="Hit Feedback")
    float FlashSeconds = 0.0f;
};

/**
 * 无状态确定性反馈策略；Moba层只调节中立参数，永远不直接播放具体资源。
 * 负值、过大输入在此归一化，绝不允许无限堆叠顿帧。
 */
class MOBAPRESENTATIONRUNTIME_API FMobaHitFeedbackPolicy
{
public:
    static FMobaHitFeedbackDecision Evaluate(
        const FMobaHitFeedbackInput& Input,
        const FGamePlatformHitFeedbackTuning& Tuning);
};

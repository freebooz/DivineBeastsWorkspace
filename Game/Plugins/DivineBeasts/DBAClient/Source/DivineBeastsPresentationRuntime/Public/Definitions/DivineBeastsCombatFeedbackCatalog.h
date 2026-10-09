#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Feedback/GamePlatformHitFeedbackProfile.h"
#include "DivineBeastsCombatFeedbackCatalog.generated.h"

/**
 * 神兽联盟英雄技能命中反馈绑定表项，归项目内容所有。
 * 只描述具体英雄/技能对平台反馈参数及逻辑资源ID的映射，不执行玩法或特效。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSPRESENTATIONRUNTIME_API FDivineBeastsCombatFeedbackEntry
{
    GENERATED_BODY()

    /** 英雄定义ID；必须与项目已发布英雄数据资产ID保持一致。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Feedback")
    FName HeroDefinitionId = NAME_None;

    /** 技能定义ID；不得使用临时序号或伤害数值识别技能。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Feedback")
    FName AbilityDefinitionId = NAME_None;

    /** 客户端反馈参数资产软引用；由GamePlatformData统一负责异步租约和预加载。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Feedback")
    TSoftObjectPtr<UGamePlatformHitFeedbackProfile> Profile;

    /** 对应平台VFX目录中的逻辑DefinitionId，不能直接存硬引用Niagara资源。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Feedback")
    FName VFXDefinitionId = NAME_None;

    /** 对应平台SFX目录中的逻辑DefinitionId，不能直接存硬引用音频资源。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Feedback")
    FName SFXDefinitionId = NAME_None;
};

/**
 * UDivineBeastsCombatFeedbackCatalog（项目技能命中反馈目录）。
 * 第三层只负责映射，第一层Profile负责参数；第二层竞技组合根负责选择与解释。
 * 此类不允许自行同步加载软引用资源，不与服务器Combat组件发生反向依赖。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSPRESENTATIONRUNTIME_API UDivineBeastsCombatFeedbackCatalog
    : public UDataAsset
{
    GENERATED_BODY()

public:
    /** 所有已登记的英雄技能映射；同一Hero+Ability键冲突必须拒绝。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Combat Feedback")
    TArray<FDivineBeastsCombatFeedbackEntry> Entries;

    /**
     * 在已加载目录中精确查询英雄与技能；失败时不改写OutEntry。
     * 无默认兜底猜测，调用方可显式选择平台基础预设。
     */
    bool TryResolve(
        FName HeroDefinitionId,
        FName AbilityDefinitionId,
        FDivineBeastsCombatFeedbackEntry& OutEntry) const;

    /** 资源发布前的目录冲突检查；检测所有无效键或重复键，返回中文原因。 */
    bool ValidateMappings(TArray<FString>& OutErrors) const;
};

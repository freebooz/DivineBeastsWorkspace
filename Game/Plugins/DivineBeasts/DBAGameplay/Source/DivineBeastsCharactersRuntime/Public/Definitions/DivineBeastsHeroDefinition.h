#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Definitions/GamePlatformHeroDefinition.h"
#include "Definitions/DivineBeastsMomentumDefinition.h"
#include "Identity/DivineBeastsZodiacIdentity.h"
#include "DivineBeastsHeroDefinition.generated.h"

/** FDivineBeastsAppearanceOptionSchema（神兽联盟角色创建外观选项Schema）。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSCHARACTERSRUNTIME_API FDivineBeastsAppearanceOptionSchema
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Appearance")
    TArray<FName> BodyVariants;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Appearance")
    TArray<FName> HeadPresets;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Character|Appearance")
    TArray<FName> SkinMarkingPresets;

    bool ValidateSelection(
        const TMap<FString, FString>& Selection,
        FString& OutError) const;
};

/**
 * UDivineBeastsHeroDefinition（神兽联盟生肖英雄定义）。
 * 创建该项目扩展类的理由：ZodiacIdentity/ZodiacTag/DisplayNameKey/ContentPackId/
 * AppearanceSchema均为神兽联盟真实项目结构，被Catalog、CharacterComponent和CreationProvider共同消费。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSCHARACTERSRUNTIME_API UDivineBeastsHeroDefinition
    : public UGamePlatformHeroDefinition
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero")
    EDivineBeastsZodiacIdentity ZodiacIdentity = EDivineBeastsZodiacIdentity::Rat;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero")
    FGameplayTag ZodiacTag;

    /** 本地化Key，不是协议身份或最终显示字符串。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero")
    FName DisplayNameKey = NAME_None;

    /** 创建页可本地化的角色介绍，由每份真实DataAsset的作者源IntroductionZh填写。
     * 属于项目双端安全文本，不引用客户端资源、不授予技能、不描述服务器权威数值。
     * 默认空值兼容旧资产；创建介绍VM会明确报告未配置，不以CDO固定文案补造作者数据。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero|Introduction", meta=(MultiLine=true))
    FText Introduction;

    /** 逻辑Content Pack（内容包）ID，不直接引用视觉资产。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero")
    FName ContentPackId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero")
    FDivineBeastsAppearanceOptionSchema AppearanceSchema;

    /** 项目核心 Momentum（气势）规则；运行时真值由 UDivineBeastsMomentumAttributeSet 持有。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero|Momentum")
    FDivineBeastsMomentumDefinition Momentum;

    /** 默认技能集合的 GamePlatformDefinition（平台主资产）逻辑编号；空值兼容旧英雄资产，
     * 但正式技能授权路径会将空值标记为缺失，不会伪造技能。角色身份模块只保存编号，
     * 不包含 GAS Ability（玩法技能类）或客户端图标等依赖。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero|Abilities")
    FName DefaultAbilitySetId = NAME_None;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    bool IsProjectDefinitionValid(FString& OutError) const;
};

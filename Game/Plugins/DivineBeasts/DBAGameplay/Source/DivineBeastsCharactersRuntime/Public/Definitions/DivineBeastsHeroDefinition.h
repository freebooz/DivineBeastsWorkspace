#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Definitions/GamePlatformHeroDefinition.h"
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

    /** 逻辑Content Pack（内容包）ID，不直接引用视觉资产。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero")
    FName ContentPackId = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|Hero")
    FDivineBeastsAppearanceOptionSchema AppearanceSchema;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    bool IsProjectDefinitionValid(FString& OutError) const;
};

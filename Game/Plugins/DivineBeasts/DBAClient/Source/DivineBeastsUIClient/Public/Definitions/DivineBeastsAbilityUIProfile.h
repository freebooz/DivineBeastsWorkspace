#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Engine/Texture2D.h"
#include "DivineBeastsAbilityUIProfile.generated.h"

/** FDivineBeastsAbilityUIEntry（单个技能的客户端表现条目）；不保存伤害、冷却或授权。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSUICLIENT_API FDivineBeastsAbilityUIEntry
{
    GENERATED_BODY()

    /** 与 GAS AbilitySet 内的 FGamePlatformId::ToString() 完全一致的技能逻辑身份。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    FName AbilityId = NAME_None;

    /** 经本地化的中文技能名称与说明；修改显示名称不修改稳定 AbilityId。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    FText Description;

    /** 真实 Texture2D 由所属 Hero ContentPack（英雄内容包）管理；异步加载 UI Bundle。 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="DivineBeasts|AbilityUI",
        meta=(AssetBundles="UI"))
    TSoftObjectPtr<UTexture2D> Icon;
};

/**
 * UDivineBeastsAbilityUIProfile（生肖英雄技能栏表现配置主资产）。
 * 主资产身份=DivineBeastsAbilityUIProfile:HeroDefinitionId；
 * ClientOnly（仅客户端），禁止作为服务器英雄定义的硬引用。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSUICLIENT_API UDivineBeastsAbilityUIProfile : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    FName HeroDefinitionId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|AbilityUI")
    TArray<FDivineBeastsAbilityUIEntry> Entries;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    /** 验证英雄身份、技能 ID 和条目唯一性；只读不加载软图标。 */
    bool ValidateProfile(FString& OutError) const;

    /** 输入完整稳定技能 ID；不存在时返回空，由界面显示明确缺省图标。 */
    const FDivineBeastsAbilityUIEntry* FindEntry(FName AbilityId) const;
};

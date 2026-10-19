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

    /** 真实Texture2D由所属英雄包拥有；保留UI Bundle作者元数据，运行时由Data普通资源租约异步持有。 */
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

    /** 用户授权的只读美术预览顺序：普通、攻击一、攻击二、终极、移动、防御、控制、特色。
     * 空数组表示未制作；非空必须恰好八个唯一的既有条目ID。不是AbilitySet或技能授予。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|AbilityUI|ArtPreview")
    TArray<FName> ArtPreviewAbilityIds;

    virtual FPrimaryAssetId GetPrimaryAssetId() const override;

    /** 验证英雄身份、技能ID、条目唯一性及非空白名称/说明；只读不加载软图标。
     * 成功清空OutError，失败给出中文原因；此门禁不证明资源已加载或技能已正式批准。 */
    bool ValidateProfile(FString& OutError) const;

    /** 输入完整稳定技能 ID；不存在时返回空，由界面显示明确缺省图标。 */
    const FDivineBeastsAbilityUIEntry* FindEntry(FName AbilityId) const;
};

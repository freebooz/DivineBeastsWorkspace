#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/DivineBeastsAbilityBalanceRow.h"
#include "DivineBeastsAbilityDefinition.generated.h"

class UDataTable;

/** EDivineBeastsAbilityKind（项目技能类型）；独立于 GUI 槽位与输入设备。 */
UENUM(BlueprintType)
enum class EDivineBeastsAbilityKind : uint8
{
    Primary,    // 普通攻击
    Active,     // 主动技能
    Passive,    // 被动技能
    Ultimate    // 终极技能
};


/**
 * FDivineBeastsEightSlotDraftAction（生肖八槽技能设计行为）。
 * 仅保存可审核的数值草案，不驱动GAS伤害、治疗、移动或Niagara表现。
 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSABILITIESRUNTIME_API FDivineBeastsEightSlotDraftAction
{
    GENERATED_BODY()

    /** 行为身份限定为 damage/movement/shield/healing/control/buff。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName ActionKindId = NAME_None;

    /** 控制使用slow/stun/silence，增益则使用其已登记设计名称。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName ModifierId = NAME_None;

    /** 数值语义身份：damage_points/distance_cm/max_health_fraction/slow_fraction/magnitude_fraction。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName MagnitudeSemanticId = NAME_None;

    /** 伤害行为自带的伤害类型；其他行为必须为空。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DamageTypeId = NAME_None;

    /** 行为幅值，其单位由 MagnitudeSemanticId 明确给出。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    float Magnitude = 0.0f;

    /** 持续秒数，瞬时行为取零。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    float DurationSeconds = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    int32 HitCount = 1;

    /** 位移行为要求服务器导航验证；其他行为必须为false。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    bool bServerNavValidation = false;

    /** 复活语义独立记录；当前96项草案均为false。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    bool bAllowRevive = false;

    /** 原始设计行为的紧凑JSON，保留当前结构未专门建模的策划字段。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft", meta=(MultiLine=true))
    FString SourcePayloadJson;
};

/**
 * UDivineBeastsAbilityDefinition（神兽联盟技能逻辑定义）。
 * 继承 GamePlatformDefinition（平台主资产）现有 LogicalId/版本，不创建第二个技能 ID。
 * 此类双端安全，只包含玩法数值表和稳定英雄身份，不能引用图标、声音或 Niagara。
 */
UCLASS(BlueprintType)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsAbilityDefinition
    : public UGamePlatformDefinitionBase
{
    GENERATED_BODY()

public:
    /** 唯一项目英雄 ID，必须属于当前 Shared（共享契约）十二生肖目录。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability")
    FName HeroDefinitionId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability")
    EDivineBeastsAbilityKind Kind = EDivineBeastsAbilityKind::Active;

    /** 数值表软引用；运行时不得在激活路径同步加载。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability",
        meta=(AssetBundles="Gameplay"))
    TSoftObjectPtr<UDataTable> BalanceTable;

    /** 第1级到最高级所对应的表行名称，实际数字以 FDivineBeastsAbilityBalanceRow 校验。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability")
    TArray<FName> LevelRowNames;


    /** 只有经过原生行为、动画与联机验收才可解除的八槽设计稿禁用位。
     * 标记为true的数据资产不得被Server作为正式GameplayAbility授予。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    bool bEightSlotDraftOnly = false;

    /** Primary/Slot1..Slot7；旧五槽及被动DataAsset默认不设置。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftSlotId = NAME_None;

    /** GAS输入标签稳定身份，不允许客户端从视觉位置反向授予技能。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FGameplayTag DraftInputTag;

    /** 输入动作身份与默认键位；默认键位仅用于展示，不覆盖玩家重绑定。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftInputActionId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftDefaultKeyId = NAME_None;

    /** 按策划显示序号1～8排序；与槽位枚举值无关。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft", meta=(ClampMin="1", ClampMax="8"))
    int32 DraftDisplayOrder = 0;

    /** 玩家可读的技能中文名称；不能以英文资源对象名代替展示文案。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FText DraftDisplayNameZh;

    /** 玩家可读的中文机制说明；一级候选值由设计真源与CSV共同校验。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft", meta=(MultiLine=true))
    FText DraftDescriptionZh;

#if WITH_EDITORONLY_DATA
    /** 供策划/美术审核的中文效果概念图提示词；编辑器保存，Cook剥离，不作为玩家文案或已制作特效。 */
    UPROPERTY(EditDefaultsOnly, Category="DivineBeasts|Ability|Draft", meta=(MultiLine=true))
    FString DraftDesignPromptZh;
#endif

    /** 角色定位与目标行为仅作策划语义，必须另经服务器运行规则落实。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftRoleId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftTargetModeId = NAME_None;

    /** 资产归属及设计目录，阻止作者脚本接管旧正式技能或其他目录资源。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftAssetOwnerId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftCatalogId = NAME_None;

    /** 规范化设计、数值行与输入槽内容的SHA-256摘要。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FString DraftSourceSha256;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    TArray<FDivineBeastsEightSlotDraftAction> DraftActions;

    /** 只持有VFX语义与视觉DNA身份，禁止硬引用客户端Niagara或贴图。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftHeroVFXProfileId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftVFXCatalogKey = NAME_None;

    /** 图标仅保存未绑定占位语义；真实客户端贴图归后续表现配置，不进入服务器定义。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftIconPlaceholderId = NAME_None;

    /** VFX仅保存未绑定占位语义，不以占位身份冒充已存在的Niagara资源。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftVFXPlaceholderId = NAME_None;

    /** 当前草案没有真实图标或特效绑定；只有另行完成资源验收后才能迁移该状态。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    bool bDraftPresentationUnbound = true;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftPresentationSemanticId = NAME_None;

    /** Cast/Release/Flight/Impact/Cancel等完整表现语义列表，不含Niagara资产引用。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    TArray<FName> DraftPresentationSemanticIds;

    /** 仅验证静态引用与逻辑字段，不访问 UObject 软资源。 */
    virtual FGamePlatformResult ValidateDefinition() const override;

    /** 仅使用已预加载的表解析等级；不进行任何同步资源加载。 */
    bool TryGetLoadedBalance(int32 Level, FDivineBeastsAbilityBalanceRow& OutRow, FString& OutError) const;
};


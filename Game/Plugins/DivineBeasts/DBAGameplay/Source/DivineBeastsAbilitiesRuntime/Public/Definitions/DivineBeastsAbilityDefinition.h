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

    /** 伤害为基础点数；护盾/治疗/增益为比例；位移为厘米。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    float Magnitude = 0.0f;

    /** 持续秒数，瞬时行为取零。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    float DurationSeconds = 0.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    int32 HitCount = 1;
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
    /** V6原生技能行为已独立签审；默认false，必须与DraftOnly互斥。
     * 人工审批、GA/GE和战斗流程实际验收后才能在原生DataAsset中开启。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|V6")
    bool bV6NativeApproved = false;


    /** Primary/Slot1..Slot7；旧五槽及被动DataAsset默认不设置。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftSlotId = NAME_None;

    /** GAS输入标签稳定身份，不允许客户端从视觉位置反向授予技能。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FGameplayTag DraftInputTag;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FText DraftDisplayNameZh;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft", meta=(MultiLine=true))
    FText DraftDescriptionZh;

    /** 角色定位与目标行为仅作策划语义，必须另经服务器运行规则落实。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftRoleId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftTargetModeId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    TArray<FDivineBeastsEightSlotDraftAction> DraftActions;

    /** 只持有VFX语义与视觉DNA身份，禁止硬引用客户端Niagara或贴图。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftHeroVFXProfileId = NAME_None;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="DivineBeasts|Ability|Draft")
    FName DraftPresentationSemanticId = NAME_None;

    /** 仅验证静态引用与逻辑字段，不访问 UObject 软资源。 */
    virtual FGamePlatformResult ValidateDefinition() const override;

    /** 仅使用已预加载的表解析等级；不进行任何同步资源加载。 */
    bool TryGetLoadedBalance(int32 Level, FDivineBeastsAbilityBalanceRow& OutRow, FString& OutError) const;
};

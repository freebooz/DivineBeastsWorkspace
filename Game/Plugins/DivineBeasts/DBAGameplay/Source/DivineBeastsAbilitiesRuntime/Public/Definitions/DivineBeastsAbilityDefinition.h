#pragma once

#include "CoreMinimal.h"
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

    /** 仅验证静态引用与逻辑字段，不访问 UObject 软资源。 */
    virtual FGamePlatformResult ValidateDefinition() const override;

    /** 仅使用已预加载的表解析等级；不进行任何同步资源加载。 */
    bool TryGetLoadedBalance(int32 Level, FDivineBeastsAbilityBalanceRow& OutRow, FString& OutError) const;
};

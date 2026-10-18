// SSLM V6独立生肖固有被动审核资产：只承载双端可读的权威数值，不包含客户端VFX和编辑器图标。
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DivineBeastsV6PassiveDefinition.generated.h"

/** 当前仅两类被动拥有可执行的底层GAS结算源码；其他十类未批准并保持失败关闭。 */
UENUM(BlueprintType)
enum class EDivineBeastsV6PassiveMode : uint8
{
    Unsupported,
    TigerFracture,
    BoarBattleWill
};

/** V6被动单实例规则：按实际受击／确认命中授予有期GE，参数需由后续人工签批的正式资产提供。 */
USTRUCT(BlueprintType)
struct DIVINEBEASTSABILITIESRUNTIME_API FDivineBeastsV6PassiveRules
{
    GENERATED_BODY()

    /** 持续时长（秒）；0默认禁止启用，不能隐式用硬编码数值作为最终平衡。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Passive")
    float DurationSeconds = 0.0f;

    /** 每个独立GE来源的叠层上限：不超过5。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Passive")
    int32 MaxStacks = 0;

    /** 每层GAS数值：虎为减少目标DamageReduction；猪为增加自身DamageBonus。单位为绝对伤害点。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Passive")
    float PerStackMagnitude = 0.0f;

    /** 触发的最低实际受伤／命中点数，避免0伤害也积累；不超过100。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Passive")
    float MinimumConfirmedDamage = 0.0f;
};

/** 正式签审前数据资产永远未批准，默认不启用；不凭12生肖名称自动套用被动。 */
UCLASS(BlueprintType)
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsV6PassiveDefinition final : public UDataAsset
{
    GENERATED_BODY()
public:
    /** 稳定HeroID（英雄定义身份），不得从客户端文本或图标推断。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Passive")
    FName HeroDefinitionId = NAME_None;

    /** 当前仅Tiger.Fracture或Boar.BattleWill可选，旧别名不自动映射。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Passive")
    EDivineBeastsV6PassiveMode Mode = EDivineBeastsV6PassiveMode::Unsupported;

    /** 必须单独人工批准且加载自正式GamePlatformData租约的资产，草案默认false。 */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Passive")
    bool bApprovedForRuntime = false;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="SSLM|Passive")
    FDivineBeastsV6PassiveRules Rules;

    /** 不更改任何游戏状态的纯字段预检；来源及Avatar资格由运行组件再核验。 */
    bool ValidateForHero(FName ExpectedHero, FString& OutError) const;
};

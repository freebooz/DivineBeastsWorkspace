// SSLM V6服务器被动GE：不创建新伤害系统，使用已有平台GAS增伤/减伤属性。
#pragma once

#include "CoreMinimal.h"
#include "GameplayEffect.h"
#include "DivineBeastsV6PassiveGameplayEffects.generated.h"

/** 虎裂甲：给目标DamageReduction添加负值，每一来源独立最多五层，由GAS自身定时删除。 */
UCLASS()
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsV6TigerFractureGameplayEffect final
    : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UDivineBeastsV6TigerFractureGameplayEffect();
    static FName MagnitudeDataName();
};

/** 猪战意：自己的DamageBonus有期正增益，同目标聚合并受限叠层。 */
UCLASS()
class DIVINEBEASTSABILITIESRUNTIME_API UDivineBeastsV6BoarBattleWillGameplayEffect final
    : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UDivineBeastsV6BoarBattleWillGameplayEffect();
    static FName MagnitudeDataName();
};

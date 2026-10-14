#pragma once

#include "GameplayEffect.h"
#include "DivineBeastsMomentumGainGameplayEffect.generated.h"

/**
 * UDivineBeastsMomentumGainGameplayEffect（SSLM基础攻击命中增势效果）。
 * 仅由服务器已验证的战斗结算事件触发；使用SetByCaller名称传入英雄定义批准的正向数值。
 * 不在GE本体缓存角色、世界或事件，也不向客户端提供直接施加入口。
 */
UCLASS()
class UDivineBeastsMomentumGainGameplayEffect final : public UGameplayEffect
{
    GENERATED_BODY()
public:
    UDivineBeastsMomentumGainGameplayEffect();
    /** 单一稳定的GAS SetByCaller名称，不属于已废止的气势倍率属性。 */
    static FName GetMagnitudeDataName();
};

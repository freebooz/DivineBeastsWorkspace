// 项目服务器安全数值门禁；供预热与授权调用，不拥有GAS属性或执行任何资源扣减。
#include "Types/DivineBeastsAbilityBalanceRow.h"
#include "Types/GamePlatformId.h"

bool FDivineBeastsAbilityBalanceRow::Validate(FString& OutError) const
{
    // 先验证跨服务稳定 ID，而不是仅判断 FName 非空，以免资产重命名改变身份。
    FGamePlatformId ParsedId;
    if (AbilityId.IsNone() || !FGamePlatformId::TryParse(AbilityId.ToString(), ParsedId))
    {
        OutError = TEXT("技能数值行的 AbilityId 必须是合法平台逻辑身份。");
        return false;
    }
    if (Level < 1 || Level > 100)
    {
        OutError = TEXT("技能数值行等级必须为1～100。");
        return false;
    }

    // 非有限值不得进入伤害、冷却或客户端提示计算；上限抑制错误表导致溢出。
    const float Values[] = {
        BaseDamage, CooldownSeconds, MomentumCost, CastRangeCm, AreaRadiusCm
    };
    for (const float Value : Values)
    {
        if (!FMath::IsFinite(Value) || Value < 0.0f || Value > 100000000.0f)
        {
            OutError = TEXT("技能数值存在负数、非有限值或超出安全范围的数值。");
            return false;
        }
    }
    if (CooldownSeconds > 3600.0f || MomentumCost > 100000.0f ||
        CastRangeCm > 1000000.0f || AreaRadiusCm > 1000000.0f)
    {
        OutError = TEXT("技能冷却、气势成本或范围超过当前项目约束。");
        return false;
    }

    const int32 TypeIndex = static_cast<int32>(DamageType);
    if (TypeIndex < static_cast<int32>(EGamePlatformDamageType::Untyped) ||
        TypeIndex > static_cast<int32>(EGamePlatformDamageType::TrueDamage))
    {
        OutError = TEXT("技能数值行使用了不受支持的伤害类型。");
        return false;
    }

    // 2026-10-14用户明确法术不消耗气势。错误或旧表必须在授权前拒绝，
    // 不能只隐藏UI费用，让服务器继续执行过期成本。
    if (DamageType == EGamePlatformDamageType::Magic && MomentumCost != 0.0f)
    {
        OutError = TEXT("法术数值行的气势费用必须为零。");
        return false;
    }

    OutError.Reset();
    return true;
}

// SSLM V6固有被动审批数据字段校验。
#include "Definitions/DivineBeastsV6PassiveDefinition.h"

bool UDivineBeastsV6PassiveDefinition::ValidateForHero(
    FName ExpectedHero, FString& OutError) const
{
    OutError.Reset();
    if (!bApprovedForRuntime || ExpectedHero.IsNone() || HeroDefinitionId != ExpectedHero)
    {
        OutError = TEXT("V6固有被动无人工批准的正式资产，或英雄身份不匹配。");
        return false;
    }
    const bool bTiger = Mode == EDivineBeastsV6PassiveMode::TigerFracture;
    const bool bBoar = Mode == EDivineBeastsV6PassiveMode::BoarBattleWill;
    if ((!bTiger && !bBoar) ||
        HeroDefinitionId != (bTiger ? FName(TEXT("Hero.Zodiac.Tiger")) :
                                     FName(TEXT("Hero.Zodiac.Boar"))) ||
        !FMath::IsFinite(Rules.DurationSeconds) || Rules.DurationSeconds < 0.2f ||
        Rules.DurationSeconds > 10.0f || Rules.MaxStacks < 1 || Rules.MaxStacks > 5 ||
        !FMath::IsFinite(Rules.PerStackMagnitude) || Rules.PerStackMagnitude <= 0.0f ||
        Rules.PerStackMagnitude > 10.0f ||
        !FMath::IsFinite(Rules.MinimumConfirmedDamage) ||
        Rules.MinimumConfirmedDamage <= 0.0f || Rules.MinimumConfirmedDamage > 100.0f)
    {
        OutError = TEXT("V6被动的角色、叠层、持续时间或GAS增减伤参数无效。");
        return false;
    }
    return true;
}

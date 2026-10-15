// GamePlatform通用VFX可选WorldId/画质请求软预算，结合已有平台硬限和Niagara原生Effect Type。
#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"

namespace
{
const FGamePlatformVFXWorldBudgetRule* SelectWorldBudgetRule(
    const FGamePlatformVFXRequest& Request, const UGamePlatformVFXSettings& Settings)
{
    if (!Settings.bUseWorldBudgetRules || Request.WorldId.IsNone()) return nullptr;

    // 仅匹配显式配置的命名空间及画质；最长前缀优先，拒绝意外同名字头匹配。
    const FString WorldId = Request.WorldId.ToString();
    const FGamePlatformVFXWorldBudgetRule* Best = nullptr;
    int32 BestLength = 0;
    for (const FGamePlatformVFXWorldBudgetRule& Rule : Settings.WorldBudgetRules)
    {
        if (Rule.QualityTier != Request.QualityTier || Rule.WorldIdPrefix.IsEmpty()) continue;
        const FString& Prefix = Rule.WorldIdPrefix;
        if (!WorldId.StartsWith(Prefix, ESearchCase::IgnoreCase)) continue;
        const int32 Length = Prefix.Len();
        if (WorldId.Len() > Length && WorldId[Length] != TEXT('.') &&
            WorldId[Length] != TEXT('/') && WorldId[Length] != TEXT('@')) continue;
        if (Length > BestLength)
        {
            Best = &Rule;
            BestLength = Length;
        }
    }
    return Best;
}
}

bool FGamePlatformVFXScalabilityPolicy::ShouldSpawn(
    const FGamePlatformVFXRequest& Request,
    int32 ActiveInstances,
    const UGamePlatformVFXSettings& Settings)
{
    // 平台HardLimit和Critical原有规则始终保留；关键战斗信息还需非Niagara的保底层。
    const int32 HardLimit = FMath::Max(1, Settings.HardMaxTrackedInstances);
    if (ActiveInstances >= HardLimit) return false;
    if (Request.Importance == EGamePlatformVFXImportance::Critical) return true;

    int32 CombatLimit = FMath::Clamp(Settings.MaxActiveInstances, 1, HardLimit);
    int32 StatusLimit = FMath::Clamp(Settings.MaxStatusInstances, 1, CombatLimit);
    int32 AmbientLimit = FMath::Clamp(Settings.MaxAmbientInstances, 1, StatusLimit);

    if (const FGamePlatformVFXWorldBudgetRule* Rule = SelectWorldBudgetRule(Request, Settings))
    {
        // World规则只能收紧，不能越过原平台保护上限；这些数字不是粒子/真实System数量。
        CombatLimit = FMath::Clamp(Rule->MaxCombatRequests, 1, CombatLimit);
        StatusLimit = FMath::Clamp(Rule->MaxStatusRequests, 1, FMath::Min(StatusLimit, CombatLimit));
        AmbientLimit = FMath::Clamp(Rule->MaxAmbientRequests, 1, FMath::Min(AmbientLimit, StatusLimit));
    }

    switch (Request.Importance)
    {
    case EGamePlatformVFXImportance::Ambient: return ActiveInstances < AmbientLimit;
    case EGamePlatformVFXImportance::Status: return ActiveInstances < StatusLimit;
    case EGamePlatformVFXImportance::Combat:
    default: return ActiveInstances < CombatLimit;
    }
}

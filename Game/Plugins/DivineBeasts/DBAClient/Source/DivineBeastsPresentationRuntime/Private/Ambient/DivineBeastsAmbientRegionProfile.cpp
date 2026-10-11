#include "Ambient/DivineBeastsAmbientRegionProfile.h"

namespace
{
/** 私有选择助手：使风格与Definition身份共用同一随机区间，避免不同资源映射。 */
const FDivineBeastsAmbientStyleWeight* SelectWeightedEntry(
    const TArray<FDivineBeastsAmbientStyleWeight>& Entries,
    const float NormalizedSample)
{
    double TotalWeight = 0.0;
    for (const FDivineBeastsAmbientStyleWeight& Entry : Entries)
    {
        if (!Entry.StyleId.IsNone() && !Entry.DefinitionId.IsNone() &&
            FMath::IsFinite(Entry.Weight) && Entry.Weight > 0.f)
        {
            TotalWeight += static_cast<double>(Entry.Weight);
        }
    }
    if (TotalWeight <= 0.0)
    {
        return nullptr;
    }

    const double Sample = FMath::IsFinite(NormalizedSample)
        ? static_cast<double>(FMath::Clamp(NormalizedSample, 0.f, 1.f)) : 0.0;
    double Target = Sample * TotalWeight;
    const FDivineBeastsAmbientStyleWeight* LastValid = nullptr;
    for (const FDivineBeastsAmbientStyleWeight& Entry : Entries)
    {
        if (Entry.StyleId.IsNone() || Entry.DefinitionId.IsNone() ||
            !FMath::IsFinite(Entry.Weight) || Entry.Weight <= 0.f)
        {
            continue;
        }
        LastValid = &Entry;
        if (Target < static_cast<double>(Entry.Weight))
        {
            return &Entry;
        }
        Target -= static_cast<double>(Entry.Weight);
    }
    return LastValid; // 边界1.0及浮点误差：返回最后有效身份。
}
}

FName UDivineBeastsAmbientRegionProfile::SelectStyle(const float NormalizedSample) const
{
    const FDivineBeastsAmbientStyleWeight* Selected =
        SelectWeightedEntry(Styles, NormalizedSample);
    return Selected ? Selected->StyleId : NAME_None;
}

FName UDivineBeastsAmbientRegionProfile::SelectDefinitionId(const float NormalizedSample) const
{
    const FDivineBeastsAmbientStyleWeight* Selected =
        SelectWeightedEntry(Styles, NormalizedSample);
    return Selected ? Selected->DefinitionId : NAME_None;
}

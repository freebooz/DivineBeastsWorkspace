#include "Ambient/DivineBeastsAmbientRegionProfile.h"

FName UDivineBeastsAmbientRegionProfile::SelectStyle(float NormalizedSample) const
{
    // 仅按已配置项目数据选择，不触发Niagara、服务器RPC或游戏世界修改。
    double TotalWeight = 0.0;
    for (const FDivineBeastsAmbientStyleWeight& Entry : Styles)
    {
        if (!Entry.StyleId.IsNone() && FMath::IsFinite(Entry.Weight) && Entry.Weight > 0.f)
        {
            TotalWeight += static_cast<double>(Entry.Weight);
        }
    }
    if (TotalWeight <= 0.0)
    {
        return NAME_None;
    }

    const double Sample = FMath::IsFinite(NormalizedSample)
        ? static_cast<double>(FMath::Clamp(NormalizedSample, 0.f, 1.f)) : 0.0;
    double Target = Sample * TotalWeight; // 恰好1.0时最终返回LastValid（稳定的最后风格）。

    FName LastValid = NAME_None;
    for (const FDivineBeastsAmbientStyleWeight& Entry : Styles)
    {
        if (Entry.StyleId.IsNone() || !FMath::IsFinite(Entry.Weight) || Entry.Weight <= 0.f)
        {
            continue;
        }
        LastValid = Entry.StyleId;
        if (Target < static_cast<double>(Entry.Weight))
        {
            return Entry.StyleId;
        }
        Target -= static_cast<double>(Entry.Weight);
    }
    // 浮点边界退化路径始终保持同一数据集内稳定身份。
    return LastValid;
}

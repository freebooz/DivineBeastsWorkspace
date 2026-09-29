#include "Services/GamePlatformPCGLinearRules.h"

bool FGamePlatformPCGLinearRules::SelectSpanMeshByLength(
    float SpanLengthCm,
    TConstArrayView<FGamePlatformPCGSpanMeshRule> Rules,
    FName& OutMeshSetId)
{
    OutMeshSetId = NAME_None;
    if (!FMath::IsFinite(SpanLengthCm) || SpanLengthCm < 0.0f)
    {
        return false;
    }

    const FGamePlatformPCGSpanMeshRule* Best = nullptr;
    for (const FGamePlatformPCGSpanMeshRule& Rule : Rules)
    {
        if (Rule.MeshSetId.IsNone() || !FMath::IsFinite(Rule.MinLengthCm) || !FMath::IsFinite(Rule.MaxLengthCm) ||
            Rule.MinLengthCm < 0.0f || Rule.MaxLengthCm < Rule.MinLengthCm ||
            SpanLengthCm < Rule.MinLengthCm || SpanLengthCm > Rule.MaxLengthCm)
        {
            continue;
        }

        if (!Best)
        {
            Best = &Rule;
            continue;
        }

        const float RuleWidth = Rule.MaxLengthCm - Rule.MinLengthCm;
        const float BestWidth = Best->MaxLengthCm - Best->MinLengthCm;
        if (RuleWidth < BestWidth || (FMath::IsNearlyEqual(RuleWidth, BestWidth) && Rule.MeshSetId.LexicalLess(Best->MeshSetId)))
        {
            Best = &Rule;
        }
    }

    if (!Best)
    {
        return false;
    }

    OutMeshSetId = Best->MeshSetId;
    return true;
}

bool FGamePlatformPCGLinearRules::ShouldKeepPost(float DistanceFromLastKeptCm, float PostSpacingCm)
{
    return FMath::IsFinite(DistanceFromLastKeptCm) &&
           FMath::IsFinite(PostSpacingCm) &&
           PostSpacingCm > 0.0f &&
           DistanceFromLastKeptCm + KINDA_SMALL_NUMBER >= PostSpacingCm;
}

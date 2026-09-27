#include "Definitions/GamePlatformProgressionTrackDefinition.h"

bool UGamePlatformProgressionTrackDefinition::Validate(
    FString& OutReason) const
{
    if (ProgressionTrackId.IsNone() ||
        SubjectType == EGamePlatformProgressionSubjectType::Unknown ||
        CurveVersion < 1 ||
        MinLevel < 1 ||
        MaxLevel < MinLevel ||
        Version < 1)
    {
        OutReason = TEXT("Progression definition identity/version is invalid.");
        return false;
    }

    const int32 ExpectedCount = MaxLevel - MinLevel + 1;
    if (CumulativeXPThresholds.Num() != ExpectedCount ||
        CumulativeXPThresholds.IsEmpty() ||
        CumulativeXPThresholds[0] != 0)
    {
        OutReason = TEXT("Cumulative XP thresholds must start at zero and match level count.");
        return false;
    }

    for (int32 Index = 1;
         Index < CumulativeXPThresholds.Num();
         ++Index)
    {
        if (CumulativeXPThresholds[Index] <=
            CumulativeXPThresholds[Index - 1])
        {
            OutReason = TEXT("Cumulative XP thresholds must be strictly increasing.");
            return false;
        }
    }

    OutReason.Reset();
    return true;
}

int32 UGamePlatformProgressionTrackDefinition::CalculateLevel(
    int64 TotalXP) const
{
    if (CumulativeXPThresholds.IsEmpty())
    {
        return MinLevel;
    }

    const int64 ClampedXP =
        FMath::Clamp<int64>(
            TotalXP,
            0,
            CumulativeXPThresholds.Last());

    int32 Low = 0;
    int32 High = CumulativeXPThresholds.Num() - 1;
    int32 Best = 0;

    while (Low <= High)
    {
        const int32 Mid = Low + ((High - Low) / 2);
        if (CumulativeXPThresholds[Mid] <= ClampedXP)
        {
            Best = Mid;
            Low = Mid + 1;
        }
        else
        {
            High = Mid - 1;
        }
    }

    return FMath::Clamp(MinLevel + Best, MinLevel, MaxLevel);
}

int64 UGamePlatformProgressionTrackDefinition::GetMaxTotalXP() const
{
    return CumulativeXPThresholds.IsEmpty()
        ? 0
        : CumulativeXPThresholds.Last();
}

int64 UGamePlatformProgressionTrackDefinition::CalculateXPIntoLevel(
    int64 TotalXP) const
{
    const int32 Level = CalculateLevel(TotalXP);
    const int32 Index = Level - MinLevel;

    if (!CumulativeXPThresholds.IsValidIndex(Index))
    {
        return 0;
    }

    const int64 ClampedXP =
        FMath::Clamp<int64>(TotalXP, 0, GetMaxTotalXP());

    return FMath::Max<int64>(
        0,
        ClampedXP - CumulativeXPThresholds[Index]);
}

int64 UGamePlatformProgressionTrackDefinition::CalculateXPForNextLevel(
    int64 TotalXP) const
{
    const int32 Level = CalculateLevel(TotalXP);
    if (Level >= MaxLevel)
    {
        return 0;
    }

    const int32 Index = Level - MinLevel;
    if (!CumulativeXPThresholds.IsValidIndex(Index + 1))
    {
        return 0;
    }

    return CumulativeXPThresholds[Index + 1] -
           CumulativeXPThresholds[Index];
}

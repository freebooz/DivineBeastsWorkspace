#include "Types/GamePlatformVFXParameters.h"

void FGamePlatformVFXParameters::Append(const FGamePlatformVFXParameters& Other)
{
    for (const TPair<FName, float>& Pair : Other.FloatValues) { FloatValues.Add(Pair.Key, Pair.Value); }
    for (const TPair<FName, int32>& Pair : Other.IntValues) { IntValues.Add(Pair.Key, Pair.Value); }
    for (const TPair<FName, FVector>& Pair : Other.VectorValues) { VectorValues.Add(Pair.Key, Pair.Value); }
    for (const TPair<FName, FLinearColor>& Pair : Other.ColorValues) { ColorValues.Add(Pair.Key, Pair.Value); }
}

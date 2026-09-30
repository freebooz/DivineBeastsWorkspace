#include "Types/GamePlatformVFXParameters.h"

#define LOCTEXT_NAMESPACE "GamePlatformVFXParameters"

int32 FGamePlatformVFXParameters::Num() const
{
    return FloatParameters.Num() + VectorParameters.Num() + PositionParameters.Num() +
        ColorParameters.Num() + IntegerParameters.Num() + BooleanParameters.Num();
}

void FGamePlatformVFXParameters::Append(const FGamePlatformVFXParameters& Other)
{
    FloatParameters.Append(Other.FloatParameters);
    VectorParameters.Append(Other.VectorParameters);
    PositionParameters.Append(Other.PositionParameters);
    ColorParameters.Append(Other.ColorParameters);
    IntegerParameters.Append(Other.IntegerParameters);
    BooleanParameters.Append(Other.BooleanParameters);
}

bool FGamePlatformVFXParameterSchema::Validate(
    const FGamePlatformVFXParameters& Parameters,
    FText& OutReason,
    bool bCheckRequiredParameters) const
{
    if (MaxOverrideCount < 0 || Parameters.Num() > MaxOverrideCount)
    {
        OutReason = LOCTEXT("TooManyOverrides", "VFX参数覆盖数量超过Definition允许上限。");
        return false;
    }

    TMap<FName, const FGamePlatformVFXParameterRule*> RuleByName;
    RuleByName.Reserve(Rules.Num());
    for (const FGamePlatformVFXParameterRule& Rule : Rules)
    {
        if (Rule.Name.IsNone() || RuleByName.Contains(Rule.Name) || Rule.MinValue > Rule.MaxValue)
        {
            OutReason = LOCTEXT("InvalidSchema", "VFX参数Schema包含空名称、重复名称或非法范围。");
            return false;
        }
        RuleByName.Add(Rule.Name, &Rule);
    }

    const auto ValidateNameAndType = [&RuleByName, &OutReason](
        FName Name,
        EGamePlatformVFXParameterType Type) -> const FGamePlatformVFXParameterRule*
    {
        const FGamePlatformVFXParameterRule* const* Found = RuleByName.Find(Name);
        if (!Found || !*Found || (*Found)->Type != Type)
        {
            OutReason = FText::Format(
                LOCTEXT("UnknownParameter", "VFX请求包含Schema未允许的参数或参数类型不匹配：{0}"),
                FText::FromName(Name));
            return nullptr;
        }
        return *Found;
    };

    for (const TPair<FName, float>& Pair : Parameters.FloatParameters)
    {
        const FGamePlatformVFXParameterRule* Rule =
            ValidateNameAndType(Pair.Key, EGamePlatformVFXParameterType::Float);
        if (!Rule || !FMath::IsFinite(Pair.Value) ||
            Pair.Value < Rule->MinValue || Pair.Value > Rule->MaxValue)
        {
            if (Rule)
            {
                OutReason = FText::Format(
                    LOCTEXT("FloatOutOfRange", "VFX浮点参数越界：{0}"),
                    FText::FromName(Pair.Key));
            }
            return false;
        }
    }
    for (const TPair<FName, int32>& Pair : Parameters.IntegerParameters)
    {
        const FGamePlatformVFXParameterRule* Rule =
            ValidateNameAndType(Pair.Key, EGamePlatformVFXParameterType::Integer);
        if (!Rule || static_cast<float>(Pair.Value) < Rule->MinValue ||
            static_cast<float>(Pair.Value) > Rule->MaxValue)
        {
            if (Rule)
            {
                OutReason = FText::Format(
                    LOCTEXT("IntegerOutOfRange", "VFX整数参数越界：{0}"),
                    FText::FromName(Pair.Key));
            }
            return false;
        }
    }
    for (const TPair<FName, FVector>& Pair : Parameters.VectorParameters)
    {
        if (!ValidateNameAndType(Pair.Key, EGamePlatformVFXParameterType::Vector) ||
            Pair.Value.ContainsNaN())
        {
            return false;
        }
    }
    for (const TPair<FName, FVector>& Pair : Parameters.PositionParameters)
    {
        if (!ValidateNameAndType(Pair.Key, EGamePlatformVFXParameterType::Position) ||
            Pair.Value.ContainsNaN())
        {
            return false;
        }
    }
    for (const TPair<FName, FLinearColor>& Pair : Parameters.ColorParameters)
    {
        if (!ValidateNameAndType(Pair.Key, EGamePlatformVFXParameterType::Color) ||
            !FMath::IsFinite(Pair.Value.R) ||
            !FMath::IsFinite(Pair.Value.G) ||
            !FMath::IsFinite(Pair.Value.B) ||
            !FMath::IsFinite(Pair.Value.A))
        {
            return false;
        }
    }
    for (const TPair<FName, bool>& Pair : Parameters.BooleanParameters)
    {
        if (!ValidateNameAndType(Pair.Key, EGamePlatformVFXParameterType::Boolean))
        {
            return false;
        }
    }

    if (bCheckRequiredParameters)
    {
        const auto HasParameter = [&Parameters](const FGamePlatformVFXParameterRule& Rule)
        {
            switch (Rule.Type)
            {
            case EGamePlatformVFXParameterType::Float:
                return Parameters.FloatParameters.Contains(Rule.Name);
            case EGamePlatformVFXParameterType::Vector:
                return Parameters.VectorParameters.Contains(Rule.Name);
            case EGamePlatformVFXParameterType::Position:
                return Parameters.PositionParameters.Contains(Rule.Name);
            case EGamePlatformVFXParameterType::Color:
                return Parameters.ColorParameters.Contains(Rule.Name);
            case EGamePlatformVFXParameterType::Integer:
                return Parameters.IntegerParameters.Contains(Rule.Name);
            case EGamePlatformVFXParameterType::Boolean:
                return Parameters.BooleanParameters.Contains(Rule.Name);
            default:
                return false;
            }
        };

        for (const FGamePlatformVFXParameterRule& Rule : Rules)
        {
            if (Rule.bRequired && !HasParameter(Rule))
            {
                OutReason = FText::Format(
                    LOCTEXT("RequiredMissing", "VFX请求缺少Definition要求的参数：{0}"),
                    FText::FromName(Rule.Name));
                return false;
            }
        }
    }

    OutReason = FText::GetEmpty();
    return true;
}

#undef LOCTEXT_NAMESPACE

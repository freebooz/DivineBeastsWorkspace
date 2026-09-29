#include "Types/GamePlatformSettingTypes.h"

#include "Misc/LexFromString.h"

FGamePlatformSettingValue FGamePlatformSettingValue::MakeBool(const bool Value)
{
    FGamePlatformSettingValue Result;
    Result.Type = EGamePlatformSettingValueType::Boolean;
    Result.BoolValue = Value;
    return Result;
}

FGamePlatformSettingValue FGamePlatformSettingValue::MakeInteger(const int64 Value)
{
    FGamePlatformSettingValue Result;
    Result.Type = EGamePlatformSettingValueType::Integer;
    Result.IntegerValue = Value;
    return Result;
}

FGamePlatformSettingValue FGamePlatformSettingValue::MakeNumber(const double Value)
{
    FGamePlatformSettingValue Result;
    Result.Type = EGamePlatformSettingValueType::Number;
    Result.NumberValue = Value;
    return Result;
}

FGamePlatformSettingValue FGamePlatformSettingValue::MakeString(FString Value)
{
    FGamePlatformSettingValue Result;
    Result.Type = EGamePlatformSettingValueType::String;
    Result.StringValue = MoveTemp(Value);
    return Result;
}

FGamePlatformSettingValue FGamePlatformSettingValue::MakeName(const FName Value)
{
    FGamePlatformSettingValue Result;
    Result.Type = EGamePlatformSettingValueType::Name;
    Result.NameValue = Value;
    return Result;
}

bool FGamePlatformSettingValue::TryParse(
    const EGamePlatformSettingValueType InType,
    const FString& Text,
    FGamePlatformSettingValue& OutValue)
{
    const FString Trimmed = Text.TrimStartAndEnd();

    switch (InType)
    {
    case EGamePlatformSettingValueType::Boolean:
        if (Trimmed.Equals(TEXT("true"), ESearchCase::IgnoreCase) || Trimmed == TEXT("1"))
        {
            OutValue = MakeBool(true);
            return true;
        }
        if (Trimmed.Equals(TEXT("false"), ESearchCase::IgnoreCase) || Trimmed == TEXT("0"))
        {
            OutValue = MakeBool(false);
            return true;
        }
        return false;

    case EGamePlatformSettingValueType::Integer:
    {
        int64 Parsed = 0;
        if (!LexTryParseString(Parsed, *Trimmed))
        {
            return false;
        }
        OutValue = MakeInteger(Parsed);
        return true;
    }

    case EGamePlatformSettingValueType::Number:
    {
        double Parsed = 0.0;
        if (!LexTryParseString(Parsed, *Trimmed) || !FMath::IsFinite(Parsed))
        {
            return false;
        }
        OutValue = MakeNumber(Parsed);
        return true;
    }

    case EGamePlatformSettingValueType::String:
        if (Trimmed.Len() > GamePlatformSettingsLimits::MaxStringLength)
        {
            return false;
        }
        OutValue = MakeString(Trimmed);
        return true;

    case EGamePlatformSettingValueType::Name:
        OutValue = MakeName(FName(*Trimmed));
        return !OutValue.NameValue.IsNone();

    default:
        return false;
    }
}

bool FGamePlatformSettingValue::IsValidForType(
    const EGamePlatformSettingValueType ExpectedType) const
{
    if (Type != ExpectedType)
    {
        return false;
    }
    return Type != EGamePlatformSettingValueType::Number ||
        FMath::IsFinite(NumberValue);
}

bool FGamePlatformSettingValue::Equals(
    const FGamePlatformSettingValue& Other) const
{
    if (Type != Other.Type)
    {
        return false;
    }

    switch (Type)
    {
    case EGamePlatformSettingValueType::Boolean:
        return BoolValue == Other.BoolValue;
    case EGamePlatformSettingValueType::Integer:
        return IntegerValue == Other.IntegerValue;
    case EGamePlatformSettingValueType::Number:
        return FMath::IsNearlyEqual(NumberValue, Other.NumberValue, 1.e-6);
    case EGamePlatformSettingValueType::String:
        return StringValue == Other.StringValue;
    case EGamePlatformSettingValueType::Name:
        return NameValue == Other.NameValue;
    default:
        return false;
    }
}

FString FGamePlatformSettingValue::ToDiagnosticString(const bool bSensitive) const
{
    if (bSensitive)
    {
        return TEXT("<redacted>");
    }

    switch (Type)
    {
    case EGamePlatformSettingValueType::Boolean:
        return BoolValue ? TEXT("true") : TEXT("false");
    case EGamePlatformSettingValueType::Integer:
        return LexToString(IntegerValue);
    case EGamePlatformSettingValueType::Number:
        return FString::SanitizeFloat(NumberValue);
    case EGamePlatformSettingValueType::String:
        return StringValue;
    case EGamePlatformSettingValueType::Name:
        return NameValue.ToString();
    default:
        return TEXT("<invalid>");
    }
}

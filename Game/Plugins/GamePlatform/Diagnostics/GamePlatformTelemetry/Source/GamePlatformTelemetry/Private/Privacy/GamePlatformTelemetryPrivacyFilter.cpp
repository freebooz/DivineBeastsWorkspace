#include "Privacy/GamePlatformTelemetryPrivacyFilter.h"

namespace
{
FString NormalizeKey(FString Value)
{
    Value.ToLowerInline();
    Value.ReplaceInline(TEXT("-"), TEXT(""));
    Value.ReplaceInline(TEXT("_"), TEXT(""));
    Value.ReplaceInline(TEXT("."), TEXT(""));
    Value.ReplaceInline(TEXT(" "), TEXT(""));
    return Value;
}
}

bool FGamePlatformTelemetryPrivacyFilter::IsForbiddenKey(
    const FString& Key)
{
    const FString Normalized = NormalizeKey(Key);

    static const TArray<FString> Forbidden = {
        TEXT("password"),
        TEXT("accesstoken"),
        TEXT("refreshtoken"),
        TEXT("authorization"),
        TEXT("cookie"),
        TEXT("paymentreceipt"),
        TEXT("cardnumber"),
        TEXT("providersecret"),
        TEXT("privatekey"),
        TEXT("rawreceipt"),
        TEXT("paymenttoken"),
        TEXT("billingaddress")
    };

    return Forbidden.ContainsByPredicate(
        [&Normalized](const FString& Item)
        {
            return Normalized.Contains(Item);
        });
}

bool FGamePlatformTelemetryPrivacyFilter::IsValidEventName(
    FName EventName)
{
    if (EventName.IsNone())
    {
        return false;
    }

    const FString Name = EventName.ToString();
    TArray<FString> Segments;
    Name.ParseIntoArray(Segments, TEXT("."), true);

    if (Segments.Num() < 3 || Segments.Num() > 6)
    {
        return false;
    }

    for (const FString& Segment : Segments)
    {
        if (Segment.IsEmpty() || Segment.Len() > 48)
        {
            return false;
        }

        for (const TCHAR Ch : Segment)
        {
            if (!FChar::IsAlnum(Ch) && Ch != TEXT('_'))
            {
                return false;
            }
        }
    }
    return true;
}

EGamePlatformTelemetryRecordResult
FGamePlatformTelemetryPrivacyFilter::ValidateEvent(
    const FGamePlatformTelemetryEvent& Event,
    const FGamePlatformTelemetryEventDefinition& Definition,
    const FGamePlatformTelemetryLimits& Limits)
{
    if (!IsValidEventName(Event.EventName))
    {
        return EGamePlatformTelemetryRecordResult::InvalidEventName;
    }

    if (Event.SchemaVersion != Definition.SchemaVersion ||
        Event.SchemaVersion <= 0)
    {
        return EGamePlatformTelemetryRecordResult::InvalidSchemaVersion;
    }

    if (Event.Attributes.Num() > Limits.MaxAttributes)
    {
        return EGamePlatformTelemetryRecordResult::InvalidAttribute;
    }

    for (const FGamePlatformTelemetryAttribute& Attribute :
         Event.Attributes)
    {
        const FString Key = Attribute.Key.ToString();

        if (Attribute.Key.IsNone() ||
            Key.Len() > Limits.MaxKeyLength ||
            IsForbiddenKey(Key) ||
            Attribute.PrivacyClass ==
                EGamePlatformTelemetryPrivacyClass::Forbidden)
        {
            return EGamePlatformTelemetryRecordResult::ForbiddenAttribute;
        }

        const FGamePlatformTelemetryAttributeDefinition* AttributeDefinition =
            Definition.Attributes.Find(Attribute.Key);

        if (!AttributeDefinition || Attribute.Type != AttributeDefinition->Type ||
            AttributeDefinition->PrivacyClass == EGamePlatformTelemetryPrivacyClass::Forbidden)
        {
            return EGamePlatformTelemetryRecordResult::InvalidAttribute;
        }

        if (Attribute.Type == EGamePlatformTelemetryAttributeType::String &&
            Attribute.StringValue.Len() > FMath::Min(Limits.MaxStringLength, AttributeDefinition->MaxStringLength))
        {
            return EGamePlatformTelemetryRecordResult::InvalidAttribute;
        }
    }

    for (const TPair<FName, FGamePlatformTelemetryAttributeDefinition>& Pair : Definition.Attributes)
    {
        if (Pair.Value.bRequired && !Event.Attributes.ContainsByPredicate(
                [&Pair](const FGamePlatformTelemetryAttribute& Attribute)
                {
                    return Attribute.Key == Pair.Key;
                }))
        {
            return EGamePlatformTelemetryRecordResult::InvalidAttribute;
        }
    }

    if (EstimateEventBytes(Event) > Limits.MaxEventBytes)
    {
        return EGamePlatformTelemetryRecordResult::InvalidAttribute;
    }

    return EGamePlatformTelemetryRecordResult::Recorded;
}

EGamePlatformTelemetryRecordResult
FGamePlatformTelemetryPrivacyFilter::ValidateMetric(
    const FGamePlatformTelemetryMetric& Metric,
    const FGamePlatformTelemetryMetricDefinition& Definition)
{
    if (Metric.Name.IsNone() ||
        Metric.Type != Definition.Type ||
        !FMath::IsFinite(Metric.Value))
    {
        return EGamePlatformTelemetryRecordResult::InvalidAttribute;
    }

    static const TSet<FName> HighCardinality = {
        TEXT("player_id"),
        TEXT("session_id"),
        TEXT("character_id"),
        TEXT("match_id"),
        TEXT("server_instance_id"),
        TEXT("order_id"),
        TEXT("transaction_id"),
        TEXT("request_id")
    };

    for (const TPair<FName, FString>& Label : Metric.Labels)
    {
        if (HighCardinality.Contains(Label.Key) ||
            !Definition.AllowedLabels.Contains(Label.Key) ||
            Label.Value.Len() > 96 ||
            IsForbiddenKey(Label.Key.ToString()))
        {
            return EGamePlatformTelemetryRecordResult::MetricLabelNotAllowed;
        }
    }

    return EGamePlatformTelemetryRecordResult::Recorded;
}

int32 FGamePlatformTelemetryPrivacyFilter::EstimateEventBytes(
    const FGamePlatformTelemetryEvent& Event)
{
    int32 Bytes = 192 + Event.EventName.ToString().Len() * sizeof(TCHAR);

    for (const FGamePlatformTelemetryAttribute& Attribute :
         Event.Attributes)
    {
        Bytes += 64 + Attribute.Key.ToString().Len() * sizeof(TCHAR);
        if (Attribute.Type ==
            EGamePlatformTelemetryAttributeType::String)
        {
            Bytes += Attribute.StringValue.Len() * sizeof(TCHAR);
        }
    }
    return Bytes;
}

int32 FGamePlatformTelemetryPrivacyFilter::EstimateMetricBytes(
    const FGamePlatformTelemetryMetric& Metric)
{
    int32 Bytes = 128 + Metric.Name.ToString().Len() * sizeof(TCHAR);
    for (const TPair<FName, FString>& Label : Metric.Labels)
    {
        Bytes +=
            Label.Key.ToString().Len() * sizeof(TCHAR) +
            Label.Value.Len() * sizeof(TCHAR) +
            24;
    }
    return Bytes;
}

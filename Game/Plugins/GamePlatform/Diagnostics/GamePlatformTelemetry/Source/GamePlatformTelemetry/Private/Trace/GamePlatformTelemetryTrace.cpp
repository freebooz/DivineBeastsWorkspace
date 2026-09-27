#include "Trace/GamePlatformTelemetryTrace.h"

#include "Trace/Trace.h"

UE_TRACE_CHANNEL(GamePlatformTelemetryChannel);

UE_TRACE_EVENT_BEGIN(
    GamePlatformTelemetryTrace,
    RecordedEvent,
    NoSync)
    UE_TRACE_EVENT_FIELD(UE::Trace::WideString, EventName)
    UE_TRACE_EVENT_FIELD(uint64, Sequence)
    UE_TRACE_EVENT_FIELD(UE::Trace::WideString, CorrelationId)
UE_TRACE_EVENT_END()

UE_TRACE_EVENT_BEGIN(
    GamePlatformTelemetryTrace,
    Bookmark,
    NoSync)
    UE_TRACE_EVENT_FIELD(UE::Trace::WideString, Name)
    UE_TRACE_EVENT_FIELD(UE::Trace::WideString, CorrelationId)
UE_TRACE_EVENT_END()

void FGamePlatformTelemetryTrace::EmitEvent(
    FName EventName,
    uint64 Sequence,
    const FString& CorrelationId)
{
    const FString EventString = EventName.ToString();

    UE_TRACE_LOG(
        GamePlatformTelemetryTrace,
        RecordedEvent,
        GamePlatformTelemetryChannel)
        << RecordedEvent.EventName(*EventString)
        << RecordedEvent.Sequence(Sequence)
        << RecordedEvent.CorrelationId(*CorrelationId);
}

void FGamePlatformTelemetryTrace::EmitBookmark(
    const FString& Name,
    const FString& CorrelationId)
{
    UE_TRACE_LOG(
        GamePlatformTelemetryTrace,
        Bookmark,
        GamePlatformTelemetryChannel)
        << Bookmark.Name(*Name)
        << Bookmark.CorrelationId(*CorrelationId);
}

#if WITH_DEV_AUTOMATION_TESTS

#include "Buffer/GamePlatformTelemetryBoundedBuffer.h"
#include "HAL/PlatformTime.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformTelemetryDevelopmentLoadHarness,
    "GamePlatform.Telemetry.Performance.Development1000Records",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformTelemetryDevelopmentLoadHarness::RunTest(
    const FString&)
{
    FGamePlatformTelemetryLimits Limits;
    Limits.MaxBufferEvents = 2048;
    Limits.MaxBufferBytes = 4 * 1024 * 1024;
    Limits.MaxBatchEvents = 128;
    Limits.MaxBatchBytes = 256 * 1024;

    FGamePlatformTelemetryBoundedBuffer Buffer(Limits);

    const double StartSeconds = FPlatformTime::Seconds();

    for (int32 Index = 0; Index < 1000; ++Index)
    {
        FGamePlatformTelemetryEvent Event;
        Event.EventId = FGuid::NewGuid();
        Event.EventName =
            TEXT("Telemetry.Foundation.WorldLoaded");
        Event.Priority =
            EGamePlatformTelemetryPriority::Verbose;

        Buffer.EnqueueEvent(
            MoveTemp(Event),
            256);
    }

    const double DurationMilliseconds =
        (FPlatformTime::Seconds() - StartSeconds) * 1000.0;

    AddInfo(
        FString::Printf(
            TEXT("Development-only 1000 record enqueue duration_ms=%.3f"),
            DurationMilliseconds));

    TestTrue(
        TEXT("1000条记录后Buffer仍有界"),
        Buffer.GetDiagnostics().BufferDepth <=
            Limits.MaxBufferEvents);

    return true;
}

#endif

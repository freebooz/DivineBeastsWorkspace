#pragma once

#include "CoreMinimal.h"
#include "GamePlatformTelemetryTypes.generated.h"

UENUM(BlueprintType)
enum class EGamePlatformTelemetryPriority : uint8
{
    Verbose,
    Normal,
    CriticalTelemetry
};

UENUM(BlueprintType)
enum class EGamePlatformTelemetryPrivacyClass : uint8
{
    Operational,
    Pseudonymous,
    Sensitive,
    Forbidden
};

UENUM(BlueprintType)
enum class EGamePlatformTelemetrySamplingPolicy : uint8
{
    Always,
    DeterministicSessionSample,
    Probabilistic,
    Disabled
};

UENUM(BlueprintType)
enum class EGamePlatformTelemetryMetricType : uint8
{
    Counter,
    Gauge,
    Histogram,
    Duration
};

UENUM(BlueprintType)
enum class EGamePlatformTelemetryAttributeType : uint8
{
    String,
    Int64,
    Double,
    Bool
};

UENUM(BlueprintType)
enum class EGamePlatformTelemetryRecordResult : uint8
{
    Recorded,
    Disabled,
    InvalidEventName,
    InvalidSchemaVersion,
    InvalidAttribute,
    ForbiddenAttribute,
    MetricDefinitionNotFound,
    MetricLabelNotAllowed,
    SampledOut,
    RateLimited,
    BufferFull
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryAttribute
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FName Key = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    EGamePlatformTelemetryAttributeType Type =
        EGamePlatformTelemetryAttributeType::String;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    EGamePlatformTelemetryPrivacyClass PrivacyClass =
        EGamePlatformTelemetryPrivacyClass::Operational;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString StringValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    int64 Int64Value = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    double DoubleValue = 0.0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    bool BoolValue = false;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryContext
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString BuildVersion;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString ContentRevision;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString Platform;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString Environment;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString SourceRole;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString ServerRole;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString Region;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString MapId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString WorldId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString ExperienceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString ServerInstanceId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString MatchId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString ArenaModeId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString SessionId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString PseudonymousPlayerId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString CorrelationId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString TransactionId;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryEvent
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FGuid EventId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FName EventName = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    int32 SchemaVersion = 1;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FDateTime TimestampUtc;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    double MonotonicTimestampSeconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 Sequence = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    EGamePlatformTelemetryPriority Priority =
        EGamePlatformTelemetryPriority::Normal;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FGamePlatformTelemetryContext Context;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    TArray<FGamePlatformTelemetryAttribute> Attributes;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryMetric
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FName Name = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    EGamePlatformTelemetryMetricType Type =
        EGamePlatformTelemetryMetricType::Counter;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    FString Unit;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    double Value = 0.0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FDateTime TimestampUtc;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FGamePlatformTelemetryContext Context;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry")
    TMap<FName, FString> Labels;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryBatch
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FGuid BatchId;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int32 SchemaVersion = 1;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FGamePlatformTelemetryContext SourceContext;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FDateTime CreatedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    TArray<FGamePlatformTelemetryEvent> Events;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    TArray<FGamePlatformTelemetryMetric> Metrics;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 DroppedSinceLastBatch = 0;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryLimits
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="1"))
    int32 MaxAttributes = 24;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="8"))
    int32 MaxKeyLength = 64;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="16"))
    int32 MaxStringLength = 512;

    /** 单个稳定上下文字段最大字符数；上下文由平台截断为安全上限，避免异常ID放大每条记录与批次。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="32"))
    int32 MaxContextStringLength = 160;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="128"))
    int32 MaxEventBytes = 8192;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="16"))
    int32 MaxBufferEvents = 4096;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="1024"))
    int32 MaxBufferBytes = 4 * 1024 * 1024;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="1"))
    int32 MaxBatchEvents = 128;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="1024"))
    int32 MaxBatchBytes = 256 * 1024;

    /** 单次主动刷新最多提交的批次数；必须小于NetworkSink待发送容量，避免一次主线程刷新制造突发请求。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="1", ClampMax="16"))
    int32 MaxFlushBatchesPerPass = 4;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="0.1"))
    float FlushIntervalSeconds = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Telemetry", meta=(ClampMin="0.0"))
    float ShutdownFlushBudgetSeconds = 0.5f;
};

USTRUCT(BlueprintType)
struct GAMEPLATFORMTELEMETRY_API FGamePlatformTelemetryDiagnostics
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int32 BufferDepth = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 BufferBytes = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 RecordedTotal = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 DroppedVerbose = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 DroppedNormal = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 DroppedCritical = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 SampledOutTotal = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 RateLimitedTotal = 0;

    /** 当前遥测开关和一次性刷新调度状态；用于Debug/UI只读诊断。 */
    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    bool bEnabled = false;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    bool bFlushScheduled = false;

    /** Sink健康只以稳定文本摘要暴露，不向上层泄漏具体NetworkSink实现类型。 */
    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FName SinkHealth = NAME_None;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FString SinkLastError;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int32 PendingNetworkBatches = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 SubmittedBatches = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 FailedBatches = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int64 DroppedBatches = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FDateTime LastFlushUtc;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    int32 LastFlushRecords = 0;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FDateTime SinkLastSuccessUtc;

    UPROPERTY(BlueprintReadOnly, Category="Telemetry")
    FDateTime SinkLastFailureUtc;
};

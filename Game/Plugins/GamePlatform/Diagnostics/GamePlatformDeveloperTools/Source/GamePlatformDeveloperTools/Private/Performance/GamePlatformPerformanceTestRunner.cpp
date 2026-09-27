#include "Performance/GamePlatformPerformanceTestRunner.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

FName FGamePlatformPerformanceExecutorRegistry::RegisteredOwner = NAME_None;
FGamePlatformPerformanceScenarioExecutor
    FGamePlatformPerformanceExecutorRegistry::RegisteredExecutor;

bool FGamePlatformPerformanceExecutorRegistry::Register(
    FName Owner,
    FGamePlatformPerformanceScenarioExecutor Executor)
{
    if (Owner.IsNone() || !Executor)
    {
        return false;
    }

    if (!RegisteredOwner.IsNone() && RegisteredOwner != Owner)
    {
        return false;
    }

    RegisteredOwner = Owner;
    RegisteredExecutor = MoveTemp(Executor);
    return true;
}

void FGamePlatformPerformanceExecutorRegistry::Unregister(FName Owner)
{
    if (Owner == RegisteredOwner)
    {
        RegisteredExecutor = FGamePlatformPerformanceScenarioExecutor();
        RegisteredOwner = NAME_None;
    }
}

bool FGamePlatformPerformanceExecutorRegistry::IsRegistered()
{
    return !RegisteredOwner.IsNone() && static_cast<bool>(RegisteredExecutor);
}

bool FGamePlatformPerformanceExecutorRegistry::Execute(
    const FGamePlatformPerformanceProfile& Profile,
    TMap<FName, TArray<double>>& OutMetricSamples,
    FString& OutError)
{
    if (!IsRegistered())
    {
        OutError = TEXT("未注册Performance Scenario Executor（性能场景执行器）。");
        return false;
    }

    return RegisteredExecutor(Profile, OutMetricSamples, OutError);
}


bool UGamePlatformPerformanceTestRunner::IsProfileValid(
    const FGamePlatformPerformanceProfile& Profile,
    FString& OutError) const
{
    if (Profile.ScenarioId.IsNone() || !Profile.Map.IsValid())
    {
        OutError = TEXT("ScenarioId和Map必须有效。");
        return false;
    }
    if (Profile.PlayerCount < 0
        || Profile.BotCount < 0
        || Profile.DurationSeconds <= 0.0
        || Profile.WarmupSeconds < 0.0)
    {
        OutError = TEXT("Player/Bot/Duration/Warmup参数非法。");
        return false;
    }
    if (Profile.Metrics.IsEmpty())
    {
        OutError = TEXT("至少配置一个Metric（性能指标）。");
        return false;
    }
    return true;
}

double UGamePlatformPerformanceTestRunner::Percentile(
    TArray<double> Samples,
    double Quantile)
{
    if (Samples.IsEmpty())
    {
        return 0.0;
    }

    Samples.Sort();
    const double Clamped = FMath::Clamp(Quantile, 0.0, 1.0);
    const double Position = Clamped * static_cast<double>(Samples.Num() - 1);
    const int32 Lower = FMath::FloorToInt(Position);
    const int32 Upper = FMath::CeilToInt(Position);

    if (Lower == Upper)
    {
        return Samples[Lower];
    }

    const double Alpha = Position - static_cast<double>(Lower);
    return FMath::Lerp(Samples[Lower], Samples[Upper], Alpha);
}

bool UGamePlatformPerformanceTestRunner::RunProfile(
    const FGamePlatformPerformanceProfile& Profile,
    const FString& BuildVersion,
    const FString& ContentRevision,
    const FString& EngineVersion,
    const FString& HardwareProfile,
    const FGamePlatformPerformanceScenarioExecutor& ScenarioExecutor,
    FGamePlatformPerformanceRunResult& OutResult,
    FString& OutError) const
{
    if (!IsProfileValid(Profile, OutError))
    {
        return false;
    }
    if (!ScenarioExecutor)
    {
        OutError = TEXT("未提供Performance Scenario Executor（性能场景执行器）。");
        return false;
    }
    if (HardwareProfile.IsEmpty() || EngineVersion.IsEmpty())
    {
        OutError = TEXT("HardwareProfile和EngineVersion不能为空。");
        return false;
    }

    TMap<FName, TArray<double>> MetricSamples;
    if (!ScenarioExecutor(Profile, MetricSamples, OutError))
    {
        return false;
    }

    OutResult = FGamePlatformPerformanceRunResult();
    OutResult.RunId = FGuid::NewGuid().ToString(EGuidFormats::Digits);
    OutResult.ScenarioId = Profile.ScenarioId;
    OutResult.BuildVersion = BuildVersion;
    OutResult.ContentRevision = ContentRevision;
    OutResult.EngineVersion = EngineVersion;
    OutResult.HardwareProfile = HardwareProfile;

    for (const FName Metric : Profile.Metrics)
    {
        const TArray<double>* Samples = MetricSamples.Find(Metric);
        if (!Samples || Samples->IsEmpty())
        {
            OutError = FString::Printf(
                TEXT("性能场景没有返回指标样本：%s"),
                *Metric.ToString());
            return false;
        }

        FGamePlatformPerformanceMetricSummary Summary;
        Summary.Metric = Metric;
        Summary.Samples = Samples->Num();
        Summary.P50 = Percentile(*Samples, 0.50);
        Summary.P95 = Percentile(*Samples, 0.95);
        Summary.P99 = Percentile(*Samples, 0.99);
        OutResult.Metrics.Add(MoveTemp(Summary));
    }

    return true;
}

bool UGamePlatformPerformanceTestRunner::WriteResultJson(
    const FGamePlatformPerformanceRunResult& Result,
    const FString& OutputPath,
    FString& OutError) const
{
    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("runId"), Result.RunId);
    Root->SetStringField(TEXT("scenarioId"), Result.ScenarioId.ToString());
    Root->SetStringField(TEXT("buildVersion"), Result.BuildVersion);
    Root->SetStringField(TEXT("contentRevision"), Result.ContentRevision);
    Root->SetStringField(TEXT("engineVersion"), Result.EngineVersion);
    Root->SetStringField(TEXT("hardwareProfile"), Result.HardwareProfile);

    TArray<TSharedPtr<FJsonValue>> Metrics;
    for (const FGamePlatformPerformanceMetricSummary& Summary : Result.Metrics)
    {
        TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("metric"), Summary.Metric.ToString());
        Item->SetNumberField(TEXT("p50"), Summary.P50);
        Item->SetNumberField(TEXT("p95"), Summary.P95);
        Item->SetNumberField(TEXT("p99"), Summary.P99);
        Item->SetNumberField(TEXT("samples"), Summary.Samples);
        Metrics.Add(MakeShared<FJsonValueObject>(Item));
    }
    Root->SetArrayField(TEXT("metrics"), Metrics);

    FString Json;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Json);
    if (!FJsonSerializer::Serialize(Root, Writer))
    {
        OutError = TEXT("performance.json序列化失败。");
        return false;
    }

    if (!FFileHelper::SaveStringToFile(Json, *OutputPath))
    {
        OutError = FString::Printf(TEXT("performance.json写入失败：%s"), *OutputPath);
        return false;
    }

    return true;
}

bool UGamePlatformPerformanceTestRunner::CanCompareBaselines(
    const FGamePlatformPerformanceBaseline& Baseline,
    const FGamePlatformPerformanceBaseline& Candidate,
    FString& OutReason) const
{
    if (Baseline.ScenarioId != Candidate.ScenarioId)
    {
        OutReason = TEXT("ScenarioId不同，不能直接判定性能回归。");
        return false;
    }
    if (!Baseline.HardwareProfile.Equals(Candidate.HardwareProfile, ESearchCase::CaseSensitive))
    {
        OutReason = TEXT("HardwareProfile不同，不能直接判定性能回归。");
        return false;
    }
    if (!Baseline.EngineVersion.Equals(Candidate.EngineVersion, ESearchCase::CaseSensitive))
    {
        OutReason = TEXT("EngineVersion不同，需重新建立或明确迁移基线。");
        return false;
    }
    if (Baseline.Samples <= 0 || Candidate.Samples <= 0)
    {
        OutReason = TEXT("缺少样本，不能比较。");
        return false;
    }
    return true;
}

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "UObject/Object.h"
#include "GamePlatformPerformanceTestRunner.generated.h"

/** FGamePlatformPerformanceProfile（预定义性能测试场景）。 */
USTRUCT(BlueprintType)
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformPerformanceProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName ScenarioId;
    UPROPERTY(EditAnywhere) FSoftObjectPath Map;
    UPROPERTY(EditAnywhere) FName ServerRole;
    UPROPERTY(EditAnywhere) int32 PlayerCount = 1;
    UPROPERTY(EditAnywhere) int32 BotCount = 0;
    UPROPERTY(EditAnywhere) double DurationSeconds = 60.0;
    UPROPERTY(EditAnywhere) double WarmupSeconds = 10.0;
    UPROPERTY(EditAnywhere) TArray<FName> Metrics;
};

/** UGamePlatformPerformanceProfileAsset（性能测试场景配置资产）。 */
UCLASS(BlueprintType)
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformPerformanceProfileAsset : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, Category="Performance")
    FGamePlatformPerformanceProfile Profile;
};

/** FGamePlatformPerformanceMetricSummary（单指标分位汇总）。 */
USTRUCT()
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformPerformanceMetricSummary
{
    GENERATED_BODY()

    UPROPERTY() FName Metric;
    UPROPERTY() double P50 = 0.0;
    UPROPERTY() double P95 = 0.0;
    UPROPERTY() double P99 = 0.0;
    UPROPERTY() int32 Samples = 0;
};

/** FGamePlatformPerformanceRunResult（性能测试运行结果）。 */
USTRUCT()
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformPerformanceRunResult
{
    GENERATED_BODY()

    UPROPERTY() FString RunId;
    UPROPERTY() FName ScenarioId;
    UPROPERTY() FString BuildVersion;
    UPROPERTY() FString ContentRevision;
    UPROPERTY() FString EngineVersion;
    UPROPERTY() FString HardwareProfile;
    UPROPERTY() TArray<FGamePlatformPerformanceMetricSummary> Metrics;
};

/** FGamePlatformPerformanceBaseline（性能基线，针对一个主指标）。 */
USTRUCT()
struct GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformPerformanceBaseline
{
    GENERATED_BODY()

    UPROPERTY() FString BuildVersion;
    UPROPERTY() FString ContentRevision;
    UPROPERTY() FString EngineVersion;
    UPROPERTY() FString HardwareProfile;
    UPROPERTY() FName ScenarioId;
    UPROPERTY() double P50 = 0.0;
    UPROPERTY() double P95 = 0.0;
    UPROPERTY() double P99 = 0.0;
    UPROPERTY() int32 Samples = 0;
};

/**
 * FGamePlatformPerformanceScenarioExecutor（性能场景执行器）。
 * MobaCommon/DivineBeasts Editor扩展单向提供执行器，平台层不写死地图加载、Bot或竞技业务。
 */
using FGamePlatformPerformanceScenarioExecutor =
    TFunction<bool(
        const FGamePlatformPerformanceProfile& Profile,
        TMap<FName, TArray<double>>& OutMetricSamples,
        FString& OutError)>;

/**
 * FGamePlatformPerformanceExecutorRegistry（性能场景执行器注册表）。
 * 平台层只持有一个显式Owner的Editor扩展执行器，避免反向依赖具体游戏。
 */
class GAMEPLATFORMDEVELOPERTOOLS_API FGamePlatformPerformanceExecutorRegistry
{
public:
    static bool Register(
        FName Owner,
        FGamePlatformPerformanceScenarioExecutor Executor);

    static void Unregister(FName Owner);
    static bool IsRegistered();

    static bool Execute(
        const FGamePlatformPerformanceProfile& Profile,
        TMap<FName, TArray<double>>& OutMetricSamples,
        FString& OutError);

private:
    static FName RegisteredOwner;
    static FGamePlatformPerformanceScenarioExecutor RegisteredExecutor;
};

/** UGamePlatformPerformanceTestRunner（游戏平台性能测试运行器）。 */
UCLASS()
class GAMEPLATFORMDEVELOPERTOOLS_API UGamePlatformPerformanceTestRunner : public UObject
{
    GENERATED_BODY()
public:
    bool IsProfileValid(const FGamePlatformPerformanceProfile& Profile, FString& OutError) const;

    bool RunProfile(
        const FGamePlatformPerformanceProfile& Profile,
        const FString& BuildVersion,
        const FString& ContentRevision,
        const FString& EngineVersion,
        const FString& HardwareProfile,
        const FGamePlatformPerformanceScenarioExecutor& ScenarioExecutor,
        FGamePlatformPerformanceRunResult& OutResult,
        FString& OutError) const;

    bool WriteResultJson(
        const FGamePlatformPerformanceRunResult& Result,
        const FString& OutputPath,
        FString& OutError) const;

    bool CanCompareBaselines(
        const FGamePlatformPerformanceBaseline& Baseline,
        const FGamePlatformPerformanceBaseline& Candidate,
        FString& OutReason) const;

private:
    static double Percentile(TArray<double> Samples, double Quantile);
};

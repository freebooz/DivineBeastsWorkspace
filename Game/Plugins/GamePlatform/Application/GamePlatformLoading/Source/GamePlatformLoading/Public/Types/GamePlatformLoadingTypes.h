#pragma once
#include "CoreMinimal.h"
#include "Definitions/GamePlatformDefinitionBase.h"
#include "Types/GamePlatformResult.h"

class UWorld;

/** 权威就绪策略：可选失败保留诊断；降级必须执行显式回退。 */
enum class EGamePlatformLoadingRequirement : uint8 { Required, Optional, Degradable };
/** 操作终态和资源持有独立；Ready之后显式Release才释放成功任务的租约。 */
enum class EGamePlatformLoadingState : uint8 { Idle, Running, Ready, DegradedReady, Failed, Cancelled, TimedOut };
/** 每个任务尝试的实际状态；Degraded表示回退执行成功，不是忽略失败。 */
enum class EGamePlatformLoadingTaskState : uint8 { Waiting, Running, Succeeded, Failed, Cancelled, Degraded };

/** 全部字段共同校验；跨实例、跨操作及旧代次不能操作当前屏障。 */
struct FGamePlatformLoadingHandle
{
    FGuid OwnerScopeId;
    FGuid OperationId;
    uint64 Generation = 0;
    bool IsValid() const { return OwnerScopeId.IsValid() && OperationId.IsValid() && Generation != 0; }
    bool operator==(const FGamePlatformLoadingHandle& Other) const
    { return OwnerScopeId == Other.OwnerScopeId && OperationId == Other.OperationId && Generation == Other.Generation; }
};

/** 单定义租约请求；需要多定义时创建多个任务，避免隐藏的部分成功。 */
struct FGamePlatformLoadingDataRequest
{
    FPrimaryAssetId DefinitionId;
    TSubclassOf<UGamePlatformDefinitionBase> ExpectedClass = UGamePlatformDefinitionBase::StaticClass();
    TArray<FName> Bundles;
};

/** 启动后冻结。TaskType由实例工厂解析；Data使用Data字段，WorldPresence使用操作目标。 */
struct FGamePlatformLoadingTaskSpec
{
    FName TaskId;
    FName TaskType;
    EGamePlatformLoadingRequirement Requiredness = EGamePlatformLoadingRequirement::Required;
    double Weight = 1.0;
    double TimeoutSeconds = 30.0;
    TArray<FName> Dependencies;
    FGamePlatformLoadingDataRequest Data;
    /** Degradable必须同时声明实际回退工厂与独立输入，失败不自动复用坏资产。 */
    FName FallbackTaskType;
    FGamePlatformLoadingDataRequest FallbackData;
};

/** 中立目标由组合根传入，不预置项目硬路径，不承担Travel或服务器分配。 */
struct FGamePlatformLoadingOperationSpec
{
    FName Purpose;
    FString TargetWorldPackage;
    double TimeoutSeconds = 60.0;
    TArray<FGamePlatformLoadingTaskSpec> Tasks;
};

/** 值诊断；Error只允许脱敏错误码，任务错误不得包含票据/地址/个人信息。 */
struct FGamePlatformLoadingTaskSnapshot
{
    FName TaskId;
    EGamePlatformLoadingTaskState State = EGamePlatformLoadingTaskState::Waiting;
    double Progress01 = 0;
    uint64 ExecutionGeneration = 0;
    bool bIsFallback = false;
    FName Error;
};

/** 游戏线程读取后可复制；阶段用State及任务状态表达，不含世界对象或资源指针。 */
struct FGamePlatformLoadingSnapshot
{
    FGamePlatformLoadingHandle Handle;
    FName Purpose;
    FString TargetWorldPackage;
    EGamePlatformLoadingState State = EGamePlatformLoadingState::Idle;
    double StartTimeSeconds = 0;
    double DeadlineSeconds = 0;
    double OverallProgress01 = 0;
    TArray<FGamePlatformLoadingTaskSnapshot> Tasks;
    FGamePlatformResult Result;
    bool bResourcesHeld = false;
};

/** 注册/订阅撤销使用不可复用身份；服务核对归属，不能用旧句柄撤销新记录。 */
struct FGamePlatformLoadingRegistration
{
    FGuid OwnerScopeId;
    FGuid RegistrationId;
    bool IsValid() const { return OwnerScopeId.IsValid() && RegistrationId.IsValid(); }
};

/**
 * GameInstance（游戏实例）作用域的轻量运行诊断。
 * 仅保存数值，不持有任务、世界或资源对象；供Debug/Telemetry上层按需采样，Loading本身不反向依赖遥测插件。
 */
struct FGamePlatformLoadingDiagnostics
{
    /** 当前Loading服务作用域。 */
    FGuid OwnerScopeId;
    /** 当前是否已经安排下一次Ticker采样。 */
    bool bTickerScheduled = false;
    /** 当前活跃任务实例数。 */
    int32 ActiveTaskExecutions = 0;
    /** 当前注册的自定义任务工厂数。 */
    int32 RegisteredTaskFactories = 0;
    /** 当前状态订阅数。 */
    int32 SubscriberCount = 0;
    /** 本实例累计接纳的Loading操作数。 */
    int64 TotalOperationsStarted = 0;
    /** 本实例累计执行的Ticker采样次数。 */
    int64 TotalTickerExecutions = 0;
    /** 本实例累计调用任务Poll的次数。 */
    int64 TotalTaskPolls = 0;
    /** 本实例累计构造并尝试发布的状态快照数量。 */
    int64 TotalSnapshotsPublished = 0;
    /** 本实例累计执行的订阅回调数量。 */
    int64 TotalSubscriberCallbacks = 0;
    /** 最近一次Loading Ticker主体耗时，单位毫秒。 */
    double LastTickMilliseconds = 0.0;
    /** 本实例观察到的Loading Ticker最大主体耗时，单位毫秒。 */
    double MaxTickMilliseconds = 0.0;
};

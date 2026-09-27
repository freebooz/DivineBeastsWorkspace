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

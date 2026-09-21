#pragma once
#include "CoreMinimal.h"
#include "Types/GamePlatformId.h"
#include "Types/GamePlatformResult.h"
#include "UObject/PrimaryAssetId.h"

/** 请求终态和输出保留期分离；Succeeded后仍需要显式释放。 */
enum class EGamePlatformPCGPhase : uint8 { Idle, Loading, Generating, Retained, Cleaning, Cleaned };
enum class EGamePlatformPCGOutcome : uint8 { Pending, Succeeded, Failed, Cancelled, TimedOut };
/** 不可跨世界、跨操作复用，所有成员共同校验；执行组件每请求新建，绝不复用旧回调上下文。 */
struct FGamePlatformPCGHandle
{
    FGuid OwnerScopeId;
    FGuid OperationId;
    FGuid ContextGeneration;
    uint64 ExecutionGeneration = 0;
    bool IsValid() const { return OwnerScopeId.IsValid() && OperationId.IsValid() && ContextGeneration.IsValid() && ExecutionGeneration != 0; }
    bool operator==(const FGamePlatformPCGHandle& Other) const
    { return OwnerScopeId == Other.OwnerScopeId && OperationId == Other.OperationId && ContextGeneration == Other.ContextGeneration && ExecutionGeneration == Other.ExecutionGeneration; }
};
/** 运行输入：显式当前世界中的区域与位置；参数来自只读Profile，不接受任意属性字符串。 */
struct FGamePlatformPCGRequest
{
    FPrimaryAssetId ProfileId;
    FGamePlatformId RegionId;
    FGuid ContextGeneration;
    FVector CenterCm = FVector::ZeroVector;
    TWeakObjectPtr<UObject> Owner;
};
/** 不含可变原生图/组件指针或认证材料的值快照；Cleaning绝不等于资源已释放。 */
struct FGamePlatformPCGSnapshot
{
    FGamePlatformPCGHandle Handle;
    FPrimaryAssetId ProfileId;
    FGamePlatformId RegionId;
    int32 ProfileRevision = 0;
    EGamePlatformPCGPhase Phase = EGamePlatformPCGPhase::Idle;
    EGamePlatformPCGOutcome Outcome = EGamePlatformPCGOutcome::Pending;
    int32 ActualInstanceCount = 0;
    FString OutputFingerprint;
    double StartedSeconds = 0;
    double DeadlineSeconds = 0;
    double GenerationSeconds = 0;
    double CleaningSeconds = 0;
    bool bIsResultValid = false;
    FGamePlatformResult Result;
};
/** 订阅可撤销身份，不使用函数地址作为句柄。 */
struct FGamePlatformPCGSubscription { FGuid OwnerScopeId; FGuid SubscriptionId; };

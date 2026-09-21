#pragma once

#include "CoreMinimal.h"
#include "UObject/PrimaryAssetId.h"

class UGameInstance;
class UGamePlatformFlowNode;

/** 流程可观察状态。终态保存到下一次配置／启动；不自动登录、重连或恢复旧票据。 */
enum class EGamePlatformFlowState : uint8
{
    Idle, Running, RetryWaiting, Succeeded, Failed, Cancelled, Shutdown
};

/** 每次尝试的退出原因；Finish 必须清理本次资源，不能借成功跳过释放。 */
enum class EGamePlatformFlowFinishReason : uint8
{
    Succeeded, Failed, Cancelled, TimedOut, Shutdown
};

/** 作用域与代次同时匹配才可取消；不同 GameInstance 的相同 RunId 不相互有效。 */
struct FGamePlatformFlowHandle
{
    FGuid ScopeId;
    uint64 RunId = 0;
    bool IsValid() const { return ScopeId.IsValid() && RunId != 0; }
};

/** 只在线程安全完成回调中传递值，不包含 UObject、资源路径或执行命令。 */
struct FGamePlatformFlowNodeResult
{
    bool bSucceeded = false;
    bool bRetryable = false;  // 仅失败有效；仍受组合根 MaxAttempts 限制。
    FName Outcome = NAME_None; // 成功分支键；None 选择默认边。
    FName ErrorCode = NAME_None;
    FString ErrorMessage;     // 已脱敏诊断，不存 Token 或个人资料。

    static FGamePlatformFlowNodeResult Success(FName InOutcome = NAME_None)
    {
        FGamePlatformFlowNodeResult Result;
        Result.bSucceeded = true;
        Result.Outcome = InOutcome;
        return Result;
    }

    static FGamePlatformFlowNodeResult Failure(FName Code, FString Message, bool bCanRetry = false)
    {
        FGamePlatformFlowNodeResult Result;
        Result.ErrorCode = Code;
        Result.ErrorMessage = MoveTemp(Message);
        Result.bRetryable = bCanRetry;
        return Result;
    }
};

/** 执行期间由子系统保活 Payload。弱引用只能在游戏线程解引用，切图后须重新获取世界。 */
struct FGamePlatformFlowContext
{
    FGamePlatformFlowHandle Handle;
    FName NodeId = NAME_None;
    int32 Attempt = 0;
    TWeakObjectPtr<UGameInstance> GameInstance;
    TWeakObjectPtr<UObject> Payload; // 组合根自定义的强类型 UObject；平台层不解析业务字段。
    /** 本次节点进入／重试的唯一代次；循环回到同名节点也不同，0表示尚未开始。 */
    uint64 NodeGeneration = 0;
    /** 资产模式的中立输入定义身份；旧C++装配为空。追加字段保留旧聚合初始化顺序。 */
    FPrimaryAssetId InputDefinitionId;
};

/** 外部事件与精确取消令牌；四维身份全部匹配当前已开始节点才生效。 */
struct FGamePlatformFlowNodeToken
{
    FGamePlatformFlowHandle Handle;
    FName NodeId = NAME_None;
    uint64 NodeGeneration = 0;
    bool IsValid() const { return Handle.IsValid() && !NodeId.IsNone() && NodeGeneration != 0; }
};

/** 工厂撤销身份；只有创建它的GameInstance可以撤销同一注册，旧句柄不能撤销重新注册。 */
struct FGamePlatformFlowFactoryHandle
{
    FGuid ScopeId;
    FGuid RegistrationId;
    FName ExecutorId = NAME_None;
    bool IsValid() const { return ScopeId.IsValid() && RegistrationId.IsValid() && !ExecutorId.IsNone(); }
};

/** 游戏线程创建节点；每次运行必须返回本GameInstance新建的独立实例，不能返回CDO或复用旧对象。 */
using FGamePlatformFlowNodeFactory = TFunction<UGamePlatformFlowNode*(UGameInstance&)>;

/** 节点完成可从工作线程调用；执行器下一次游戏线程调度处理，只接受首次完成。 */
using FGamePlatformFlowCompletion = TFunction<void(FGamePlatformFlowNodeResult)>;

/** 节点实例由组合根 NewObject 创建，Outer 链必须属于调用方 GameInstance。 */
struct FGamePlatformFlowStep
{
    FName NodeId = NAME_None;
    UGamePlatformFlowNode* Node = nullptr; // Configure 成功后由子系统保活，运行中不允许替换。
    FName NextNodeId = NAME_None;         // 默认成功边；None 结束本次流程。
    TMap<FName, FName> Routes;            // 成功分支到下一节点，None 目标表示结束。
    double TimeoutSeconds = 30.0;        // 每次尝试限时，包含等待异步回调的时间。
    int32 MaxAttempts = 1;               // 包含首次；组合根保证重试的幂等性。
    double RetryDelaySeconds = 0.0;      // 固定退避，基于真实单调时间，不受游戏暂停影响。
    bool bRetryOnTimeout = false;
};

/** 当前阶段使用 C++ 显式装配；入口和所有边一次性校验，不依赖插件扫描顺序。 */
struct FGamePlatformFlowDefinition
{
    FName EntryNodeId = NAME_None;
    TArray<FGamePlatformFlowStep> Steps;
};

/** 查询及终态事件使用的值快照；不会自动复制到其他客户端或服务器。 */
struct FGamePlatformFlowSnapshot
{
    EGamePlatformFlowState State = EGamePlatformFlowState::Idle;
    FGamePlatformFlowHandle Handle;
    FName NodeId = NAME_None;
    int32 Attempt = 0;
    FName ErrorCode = NAME_None;
    FString ErrorMessage;
    /** 当前节点代次；结合Handle和NodeId构造外部事件令牌，0表示尚未开始。 */
    uint64 NodeGeneration = 0;
};

DECLARE_MULTICAST_DELEGATE_OneParam(FGamePlatformFlowFinished, const FGamePlatformFlowSnapshot&);

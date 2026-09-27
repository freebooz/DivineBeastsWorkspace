#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/GamePlatformFlowTypes.h"
#include "GamePlatformFlowNode.generated.h"

/**
 * 组合根注入的 C++ 节点契约。登录、准入和资源就绪节点由对应上层实现。
 * 同一节点实例不可同时注入两个步骤；跨 GameInstance 不共享节点实例。
 * 平台不推断重试安全性，节点必须确保请求幂等并在 Finish 中解除业务委托。
 */
UCLASS(Abstract, Transient)
class GAMEPLATFORMAPPLICATIONFLOW_API UGamePlatformFlowNode : public UObject
{
    GENERATED_BODY()

public:
    /**
     * 游戏线程开始一次尝试；不得阻塞或重入 Configure/Start/Cancel。
     * Context 为值副本；Complete 可移动给异步请求，可在工作线程调用。
     * UObject 访问、业务服务调用及 Finish 始终在游戏线程，不能捕获过期世界强引用。
     */
    virtual void Execute(const FGamePlatformFlowContext& Context, FGamePlatformFlowCompletion Complete)
        PURE_VIRTUAL(UGamePlatformFlowNode::Execute, );

    /**
     * 每次已开始的尝试恰好调用一次，包括成功、失败、超时、取消与作用域退出。
     * 在游戏线程同步清理本次请求、委托和租约；不得阻塞等待工作线程或把业务结果再次提交。
     * 完成后的业务数据放到组合根 Payload 或领域服务中，不依赖本节点保留临时资源。
     */
    virtual void Finish(EGamePlatformFlowFinishReason Reason)
        PURE_VIRTUAL(UGamePlatformFlowNode::Finish, );
};

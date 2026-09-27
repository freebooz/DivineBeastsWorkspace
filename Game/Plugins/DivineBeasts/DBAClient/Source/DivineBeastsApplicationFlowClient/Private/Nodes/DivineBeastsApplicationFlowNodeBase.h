#pragma once

#include "CoreMinimal.h"
#include "Interfaces/GamePlatformFlowNode.h"
#include "DivineBeastsApplicationFlowNodeBase.generated.h"

class UDivineBeastsApplicationFlowContext;

/**
 * UDivineBeastsApplicationFlowNodeBase（神兽联盟应用流程节点基类）。
 *
 * 只承载项目节点真正共享的生命周期能力：校验强类型Payload、缓存当前节点令牌、拒绝过期访问并统一清理。
 * 它不维护 CurrentState（当前状态），不执行业务 Tick，也不复制平台执行器的路由/重试逻辑。
 *
 * 性能：
 * 每个 Flow Run（流程运行）按平台工厂契约创建独立节点实例；节点仅缓存一个弱上下文和小型值令牌，
 * 不持有 World/Actor/Widget，不产生跨地图 GC 引用链。
 */
UCLASS(Abstract, Transient)
class UDivineBeastsApplicationFlowNodeBase : public UGamePlatformFlowNode
{
    GENERATED_BODY()

public:
    virtual void Finish(EGamePlatformFlowFinishReason Reason) override;

protected:
    /**
     * 每次 Execute 开始时调用。只有Payload属于当前GameInstance且类型正确才成功。
     * 成功后生成完整 FGamePlatformFlowNodeToken，供事件驱动完成/取消精确关联当前节点代次。
     */
    UDivineBeastsApplicationFlowContext* BeginExecution(
        const FGamePlatformFlowContext& Context);

    UDivineBeastsApplicationFlowContext* GetProjectContext() const
    {
        return ProjectContext.Get();
    }

    const FGamePlatformFlowNodeToken& GetNodeToken() const
    {
        return NodeToken;
    }

    bool IsExecutionActive() const
    {
        return bExecutionActive && NodeToken.IsValid() && ProjectContext.IsValid();
    }

private:
    TWeakObjectPtr<UDivineBeastsApplicationFlowContext> ProjectContext;
    FGamePlatformFlowNodeToken NodeToken;
    bool bExecutionActive = false;
};

/**
 * UDivineBeastsPassiveFlowNode（神兽联盟被动等待流程节点）。
 *
 * 用于 Authentication（认证）、CharacterEntry（角色入口）、WorldReady（世界就绪）、
 * InWorld（世界内）等需要等待外部真实事件的节点。Execute 只建立节点代次，不主动完成；
 * 项目协调子系统通过平台 SubmitEvent 精确推进，避免逐帧轮询。
 */
UCLASS(Transient)
class UDivineBeastsPassiveFlowNode final : public UDivineBeastsApplicationFlowNodeBase
{
    GENERATED_BODY()

public:
    virtual void Execute(
        const FGamePlatformFlowContext& Context,
        FGamePlatformFlowCompletion Complete) override;
};

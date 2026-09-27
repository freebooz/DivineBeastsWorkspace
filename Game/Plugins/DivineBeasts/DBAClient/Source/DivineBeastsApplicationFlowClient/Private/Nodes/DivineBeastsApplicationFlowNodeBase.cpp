#include "Nodes/DivineBeastsApplicationFlowNodeBase.h"

#include "Context/DivineBeastsApplicationFlowContext.h"
#include "Engine/GameInstance.h"

UDivineBeastsApplicationFlowContext*
UDivineBeastsApplicationFlowNodeBase::BeginExecution(
    const FGamePlatformFlowContext& Context)
{
    check(IsInGameThread());

    UGameInstance* Instance = Context.GameInstance.Get();
    UDivineBeastsApplicationFlowContext* Payload =
        Cast<UDivineBeastsApplicationFlowContext>(Context.Payload.Get());
    if (!Instance || !Payload || Payload->GetTypedOuter<UGameInstance>() != Instance ||
        !Context.Handle.IsValid() || Context.NodeId.IsNone() ||
        Context.NodeGeneration == 0)
    {
        ProjectContext.Reset();
        NodeToken = {};
        bExecutionActive = false;
        return nullptr;
    }

    ProjectContext = Payload;
    NodeToken.Handle = Context.Handle;
    NodeToken.NodeId = Context.NodeId;
    NodeToken.NodeGeneration = Context.NodeGeneration;
    bExecutionActive = true;
    return Payload;
}

void UDivineBeastsApplicationFlowNodeBase::Finish(
    EGamePlatformFlowFinishReason Reason)
{
    check(IsInGameThread());

    // 所有结束原因统一使节点代次失效；迟到业务事件随后会被平台 Token 校验拒绝。
    bExecutionActive = false;
    NodeToken = {};
    ProjectContext.Reset();
}

void UDivineBeastsPassiveFlowNode::Execute(
    const FGamePlatformFlowContext& Context,
    FGamePlatformFlowCompletion Complete)
{
    check(IsInGameThread());

    if (!BeginExecution(Context))
    {
        Complete(FGamePlatformFlowNodeResult::Failure(
            TEXT("DivineBeastsFlowContextInvalid"),
            TEXT("神兽联盟流程节点缺少当前GameInstance所属的项目流程上下文。")));
        return;
    }

    // 被动节点刻意不保存Completion，也不创建Ticker。
    // 外部真实事件统一通过平台SubmitEvent + NodeGeneration推进，避免双完成入口和轮询开销。
}

#include "Interfaces/GamePlatformCallbackFlowNode.h"

bool UGamePlatformCallbackFlowNode::Bind(FExecuteAction InExecute, FFinishAction InFinish)
{
    if (!IsInGameThread() || ExecuteAction || !InExecute || !InFinish) return false;
    ExecuteAction = MoveTemp(InExecute);
    FinishAction = MoveTemp(InFinish);
    return true;
}

void UGamePlatformCallbackFlowNode::Execute(const FGamePlatformFlowContext& Context, FGamePlatformFlowCompletion Complete)
{
    check(IsInGameThread());
    if (!ExecuteAction || bExecuting)
    {
        Complete(FGamePlatformFlowNodeResult::Failure(TEXT("NodeNotReady"), TEXT("节点未绑定或正在执行")));
        return;
    }
    bExecuting = true;
    ExecuteAction(Context, MoveTemp(Complete));
}

void UGamePlatformCallbackFlowNode::Finish(EGamePlatformFlowFinishReason Reason)
{
    check(IsInGameThread());
    if (!bExecuting) return;
    bExecuting = false;
    FinishAction(Reason);
}

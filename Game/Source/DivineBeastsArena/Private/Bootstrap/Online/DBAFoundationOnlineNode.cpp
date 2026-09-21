#include "Bootstrap/Online/DBAFoundationOnlineNode.h"
#include "Bootstrap/DBAFoundationCoordinator.h"
#include "Engine/GameInstance.h"

void UDBAFoundationOnlineNode::Configure(EDBAOnlineOperation InOperation, UDBAFoundationCoordinator& Root)
{
    check(IsInGameThread());
    check(!bActive);
    Operation = InOperation;
    Coordinator = &Root;
}

void UDBAFoundationOnlineNode::Execute(const FGamePlatformFlowContext& Context, FGamePlatformFlowCompletion Complete)
{
    Finish(EGamePlatformFlowFinishReason::Cancelled);
    Instance = Context.GameInstance;
    bActive = true;
    Completion = MoveTemp(Complete);
    auto* Root = Coordinator.Get();
    if (!Root || !Instance.IsValid() || Root->GetTypedOuter<UGameInstance>() != Instance.Get() || Context.Payload.Get() != Root)
    { CompleteOnce(FGamePlatformResult::Failure(TEXT("OnlineProjectContextInvalid"), TEXT("在线节点不属于当前运行的实例与组合根"))); return; }
    if (Operation == EDBAOnlineOperation::ValidateConfiguration)
    {
        auto Result = Root->ValidateConfiguration();
        if (Result.IsSuccess()) { Result = Root->InitializeOnlineContext(); }
        CompleteOnce(Result);
        return;
    }
    if (Operation == EDBAOnlineOperation::Ready)
    {
        Ticker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDBAFoundationOnlineNode::TickReady), 0.05f);
        return;
    }
    auto* OnlineRoot = Root->GetOnlineContext();
    if (!OnlineRoot) { CompleteOnce(FGamePlatformResult::Failure(TEXT("OnlineContextMissing"), TEXT("在线开发上下文不存在"))); return; }
    const uint64 Expected = ++Generation;
    TWeakObjectPtr<UDBAFoundationOnlineNode> WeakThis(this);
    Request = OnlineRoot->Begin(Operation, [WeakThis, Expected](FGamePlatformResult Result)
    {
        if (auto* Self = WeakThis.Get(); Self && Self->bActive && Self->Generation == Expected) { Self->CompleteOnce(MoveTemp(Result)); }
    });
}

bool UDBAFoundationOnlineNode::TickReady(float)
{
    if (!bActive) { Ticker.Reset(); return false; }
    auto* Root = Coordinator.Get();
    if (Root && Root->IsFoundationReady() && Root->GetOnlineContext() && Root->GetOnlineContext()->IsAuthenticatedWithProfile())
    {
        Ticker.Reset();
        CompleteOnce(FGamePlatformResult::Success());
        return false;
    }
    return true; // 等实际观察者、世界及数据，实例级Flow截止处理超时，不固定延时成功。
}

void UDBAFoundationOnlineNode::CompleteOnce(FGamePlatformResult Result)
{
    if (!bActive || !Completion) { return; }
    auto Done = MoveTemp(Completion);
    Done(Result.IsSuccess() ? FGamePlatformFlowNodeResult::Success() : FGamePlatformFlowNodeResult::Failure(Result.Code, Result.Message));
}

void UDBAFoundationOnlineNode::Finish(EGamePlatformFlowFinishReason)
{
    bActive = false;
    ++Generation;
    Completion = {};
    if (Ticker.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(Ticker); Ticker.Reset(); }
    if (Instance.IsValid() && Request.RequestId.IsValid())
    { if (auto* Online = IGamePlatformOnlineService::Get(*Instance.Get())) { Online->Cancel(Request); } }
    Request = {};
    Instance.Reset();
}

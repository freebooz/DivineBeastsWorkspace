#include "Bootstrap/Nodes/DBALoadingFlowNode.h"
#include "Bootstrap/DBAFoundationCoordinator.h"
#include "API/GamePlatformApplicationFlowSubsystem.h"
#include "Interfaces/IGamePlatformLoadingService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

void UDBALoadingFlowNode::Configure(UDBAFoundationCoordinator& Owner) { Coordinator = &Owner; }
void UDBALoadingFlowNode::Execute(const FGamePlatformFlowContext& Context,FGamePlatformFlowCompletion Complete)
{
    check(IsInGameThread()); Finish(EGamePlatformFlowFinishReason::Cancelled);
    Instance = Context.GameInstance;
    auto* Root = Coordinator.Get();
    auto* Loading = Instance.IsValid() ? IGamePlatformLoadingService::Get(*Instance.Get()) : nullptr;
    if (!Loading || !Root || Root->GetTypedOuter<UGameInstance>() != Instance.Get() || Context.Payload.Get() != Root)
    { Complete(FGamePlatformFlowNodeResult::Failure(TEXT("LoadingContextMissing"),TEXT("加载服务或项目实例上下文不匹配"))); return; }
    FlowToken = {Context.Handle,Context.NodeId,Context.NodeGeneration};
    FGamePlatformLoadingOperationSpec Spec;
    Spec.Purpose = TEXT("FoundationLoadingOnly"); Spec.TargetWorldPackage = Root->SandboxPackage();
    // 两份定义必须通过真实Data加载；它们由现有Foundation资产工具生成，缺失就失败。
    for (const TCHAR* Id : {TEXT("foundation.probe@1"),TEXT("foundation.flow@1")})
    {
        FGamePlatformLoadingTaskSpec Task; Task.TaskId = FName(Id); Task.TaskType = TEXT("Data");
        Task.Data.DefinitionId = FPrimaryAssetId(TEXT("GamePlatformDefinition"),FName(Id)); Spec.Tasks.Add(Task);
    }
    FGamePlatformLoadingTaskSpec WorldTask; WorldTask.TaskId = TEXT("SandboxWorld"); WorldTask.TaskType = TEXT("WorldPresence");
    Spec.Tasks.Add(WorldTask);
    FGamePlatformResult Accepted; Operation = Loading->StartLoadingOperation(Spec,this,Accepted);
    if (!Accepted.IsSuccess()) { Complete(FGamePlatformFlowNodeResult::Failure(Accepted.Code,Accepted.Message)); return; }
    bIsActive = true;
    Subscription = Loading->SubscribeLoadingState(Operation,this,[WeakThis = TWeakObjectPtr<UDBALoadingFlowNode>(this), Expected = Operation, Token = FlowToken](const auto& Snapshot)
    {
        auto* Node = WeakThis.Get();
        if (!Node || !Node->bIsActive || !(Node->Operation == Expected) || !(Snapshot.Handle == Expected) || !Node->Instance.IsValid()) { return; }
        if (Snapshot.State == EGamePlatformLoadingState::Running || Snapshot.State == EGamePlatformLoadingState::Idle) { return; }
        auto* Service = IGamePlatformLoadingService::Get(*Node->Instance.Get());
        auto* Flow = Node->Instance->GetSubsystem<UGamePlatformApplicationFlowSubsystem>();
        if (!Service || !Flow) { return; }
        // 世界声明与Data完成之间可能失去Pawn/控制器；提交Flow成功前重新核验项目事实。
        const auto* CurrentRoot = Node->Coordinator.Get();
        const bool bReady = Service->IsReadyToPlay(Expected) && CurrentRoot && CurrentRoot->IsFoundationReady();
        FGamePlatformResult Submitted;
        Flow->SubmitEvent(Token,bReady ? FGamePlatformFlowNodeResult::Success() :
            FGamePlatformFlowNodeResult::Failure(Snapshot.Result.Code.IsNone() ? FName(TEXT("LoadingNotReady")) : Snapshot.Result.Code,TEXT("资源或基础世界屏障未满足")),Submitted);
        Node->bIsActive = false;
    });
    if (!Subscription.IsValid())
    { Finish(EGamePlatformFlowFinishReason::Failed); Complete(FGamePlatformFlowNodeResult::Failure(TEXT("LoadingSubscriptionFailed"),TEXT("加载订阅未建立"))); return; }
    WorldCheck = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this,&ThisClass::CheckWorld),0.05f);
}
bool UDBALoadingFlowNode::CheckWorld(float)
{
    if (!bIsActive || !Instance.IsValid()) { WorldCheck.Reset(); return false; }
    auto* Root = Coordinator.Get(); auto* World = Instance->GetWorld();
    if (Root && World && Root->IsFoundationReady())
    {
        if (auto* Loading = IGamePlatformLoadingService::Get(*Instance.Get()))
        {
            if (Loading->ReportWorldOperable(Operation,*World).IsSuccess()) { WorldCheck.Reset(); return false; }
        }
    }
    return true;
}
void UDBALoadingFlowNode::Finish(EGamePlatformFlowFinishReason)
{
    check(IsInGameThread()); bIsActive = false;
    if (WorldCheck.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(WorldCheck); WorldCheck.Reset(); }
    if (Instance.IsValid())
    {
        if (auto* Loading = IGamePlatformLoadingService::Get(*Instance.Get()))
        { Loading->UnsubscribeLoadingState(Subscription); if (Operation.IsValid()) { Loading->ReleaseLoadingOperation(Operation); } }
    }
    Operation = {}; Subscription = {}; FlowToken = {}; Instance.Reset();
}

#include "Bootstrap/Nodes/DBAFoundationNode.h"
#include "Bootstrap/DBAFoundationCoordinator.h"
#include "Bootstrap/DBAFoundationPolicy.h"
#include "DBAFoundationProbeDefinition.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"

void UDBAFoundationNode::Configure(EDBAFoundationOperation InOperation, UDBAFoundationCoordinator& Owner)
{
    check(IsInGameThread());
    check(!bActive);
    Operation = InOperation;
    Coordinator = &Owner;
}

void UDBAFoundationNode::Execute(const FGamePlatformFlowContext& Context, FGamePlatformFlowCompletion Complete)
{
    check(IsInGameThread());
    Finish(EGamePlatformFlowFinishReason::Cancelled);
    ++Activation;
    bActive = true;
    Completion = MoveTemp(Complete);
    Instance = Context.GameInstance;
    auto* Owner = Coordinator.Get();
    if (!Owner || !Instance.IsValid() || Context.Payload.Get() != Owner ||
        Owner->GetTypedOuter<UGameInstance>() != Instance.Get())
    {
        CompleteOnce(FGamePlatformFlowNodeResult::Failure(TEXT("ProjectContextMismatch"), TEXT("开发节点的组合根、载荷与游戏实例不匹配")));
        return;
    }
    if (Operation == EDBAFoundationOperation::Boot || Operation == EDBAFoundationOperation::ValidateConfiguration)
    {
        const auto Result = Owner->ValidateConfiguration();
        CompleteOnce(Result.IsSuccess() ? FGamePlatformFlowNodeResult::Success() :
            FGamePlatformFlowNodeResult::Failure(Result.Code, Result.Message));
        return;
    }
    if (Operation == EDBAFoundationOperation::LoadProbeDefinition)
    {
        auto* Data = IGamePlatformDataService::Get(*Instance.Get());
        if (!Data || !Context.InputDefinitionId.IsValid())
        {
            CompleteOnce(FGamePlatformFlowNodeResult::Failure(TEXT("ProbeInputMissing"), TEXT("流程资产没有声明合法测试定义输入或数据服务不存在")));
            return;
        }
        const uint64 ExpectedActivation = Activation;
        TWeakObjectPtr<UDBAFoundationNode> WeakThis(this);
        FGamePlatformResult Accepted;
        // 资源最终由协调器持有，弱调用者不能使用将于下一次配置时回收的节点。
        PendingLease = Data->AcquireDefinition(Context.InputDefinitionId, UDBAFoundationProbeDefinition::StaticClass(), {},
            EGamePlatformDataLifetime::Instance, Owner,
            [WeakThis, ExpectedActivation](const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
            {
                auto* Node = WeakThis.Get();
                if (!Node || !Node->bActive || Node->Activation != ExpectedActivation) { return; }
                if (!Result.IsSuccess())
                {
                    Node->CompleteOnce(FGamePlatformFlowNodeResult::Failure(Result.Code, Result.Message));
                    return;
                }
                auto* Root = Node->Coordinator.Get();
                FGamePlatformDataLease Transfer = Lease;
                const auto Adopted = Root ? Root->AdoptProbe(Transfer) :
                    FGamePlatformResult::Failure(TEXT("CoordinatorExpired"), TEXT("测试定义完成时组合根已关闭"));
                if (Adopted.IsSuccess()) { Node->PendingLease = {}; }
                Node->CompleteOnce(Adopted.IsSuccess() ? FGamePlatformFlowNodeResult::Success() :
                    FGamePlatformFlowNodeResult::Failure(Adopted.Code, Adopted.Message));
            }, Accepted);
        if (!Accepted.IsSuccess()) { CompleteOnce(FGamePlatformFlowNodeResult::Failure(Accepted.Code, Accepted.Message)); }
        return;
    }
    if (Operation == EDBAFoundationOperation::EnterSandbox)
    {
        TravelOperation = FGuid::NewGuid().ToString(EGuidFormats::Digits);
        // 先安装监听再切图；不使用GWorld或第零号玩家，回调还要匹配URL中的本次操作身份。
        MapLoadedHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UDBAFoundationNode::OnMapLoaded);
        PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDBAFoundationNode::PollWorld), 0.05f);
        UGameplayStatics::OpenLevel(Instance.Get(), FName(UDBAFoundationCoordinator::SandboxPackage()), true,
            FString::Printf(TEXT("FoundationTravel=%s"), *TravelOperation));
        return;
    }
    // Ready可等待本地观察者真正生成，不以固定延时或打印代替屏障。超时由实例级执行器统一管理。
    PollHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UDBAFoundationNode::PollWorld), 0.05f);
}

void UDBAFoundationNode::OnMapLoaded(UWorld* World)
{
    if (!bActive || !World || World->GetGameInstance() != Instance.Get() ||
        UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) != UDBAFoundationCoordinator::SandboxPackage() ||
        FString(World->URL.GetOption(TEXT("FoundationTravel="), TEXT(""))) != TravelOperation) { return; }
    LoadedWorld = World;
}

bool UDBAFoundationNode::PollWorld(float)
{
    if (!bActive) { PollHandle.Reset(); return false; }
    bool bReady = false;
    if (Operation == EDBAFoundationOperation::Ready)
    {
        auto* Owner = Coordinator.Get();
        bReady = Owner && Owner->IsFoundationReady();
    }
    else if (UWorld* World = LoadedWorld.Get())
    {
        const FString Package = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
        const FString WorldOperation = World->URL.GetOption(TEXT("FoundationTravel="), TEXT(""));
        bReady = DBA::Foundation::AcceptsWorldReady(World->GetGameInstance() == Instance.Get() &&
            Instance.IsValid() && Instance->GetWorld() == World, World->HasBegunPlay(),
            TCHAR_TO_UTF8(*Package), TCHAR_TO_UTF8(UDBAFoundationCoordinator::SandboxPackage()),
            TCHAR_TO_UTF8(*WorldOperation), TCHAR_TO_UTF8(*TravelOperation));
    }
    if (!bReady) { return true; }
    PollHandle.Reset();
    CompleteOnce(FGamePlatformFlowNodeResult::Success());
    return false;
}

void UDBAFoundationNode::CompleteOnce(FGamePlatformFlowNodeResult Result)
{
    if (!bActive || !Completion) { return; }
    auto Notify = MoveTemp(Completion);
    Notify(MoveTemp(Result));
}

void UDBAFoundationNode::Finish(EGamePlatformFlowFinishReason)
{
    bActive = false;
    ++Activation; // 必须先失效；Release可能排队取消通知。
    Completion = {};
    if (MapLoadedHandle.IsValid()) { FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(MapLoadedHandle); MapLoadedHandle.Reset(); }
    if (PollHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(PollHandle); PollHandle.Reset(); }
    if (PendingLease.IsValid())
    {
        if (auto* GameInstance = Instance.Get())
        {
            if (auto* Data = IGamePlatformDataService::Get(*GameInstance)) { Data->ReleaseDefinition(PendingLease); }
        }
        PendingLease = {};
    }
    LoadedWorld.Reset();
    TravelOperation.Reset();
    Instance.Reset();
}

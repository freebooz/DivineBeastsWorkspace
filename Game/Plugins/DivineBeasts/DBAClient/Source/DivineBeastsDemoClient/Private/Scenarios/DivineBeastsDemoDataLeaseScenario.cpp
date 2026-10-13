// SSLM资源租约真实样板；中央Data服务实际加载Engine Cube，两个独立完整句柄验证共享/取消。
// 适配拥有自己的需求；回调弱持状态并核对操作代次，Stop先失效再释放，不卸载其他调用者。
#include "Scenarios/DivineBeastsDemoSampleAdapters.h"
#include "Demo/DivineBeastsDemoWorldSubsystem.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

namespace DivineBeasts::Demo
{
/** 仅本次适配的异步状态，实例生命周期由适配持有，完成回调不延长世界生命。 */
struct FDataLeaseScenarioState : TSharedFromThis<FDataLeaseScenarioState>
{
    FDivineBeastsDemoScenarioContext Context;
    TWeakObjectPtr<UGameInstance> Instance;
    FGamePlatformDataLease A, B, CancelledLease;
    FDivineBeastsDemoOperationCompletion PendingCompletion;
    uint64 OperationGeneration = 0;
    bool bPrepared = false;
    bool bWaiting = false;
    bool bAReady = false;
    bool bBReady = false;
    /** 只取本实例实际服务，不能创建替身或访问另一实例。 */
    IGamePlatformDataService* GetData() const { return Instance.IsValid() ? IGamePlatformDataService::Get(*Instance) : nullptr; }
    /** 先清空自己记录再释放；返回首个真实失败，其余租约仍逐项释放。 */
    FGamePlatformResult ReleaseLeases()
    {
        const TArray<FGamePlatformDataLease> Leases = {A, B, CancelledLease}; A = {}; B = {}; CancelledLease = {};
        auto* Data = GetData(); FGamePlatformResult Result = FGamePlatformResult::Success();
        for (const auto& Lease : Leases)
        {
            if (!Lease.IsValid()) continue;
            const auto Released = Data ? Data->ReleaseResources(Lease) : FGamePlatformResult::Failure(FName(TEXT("Demo.DataReleaseUnavailable")), TEXT("释放样板租约时真实服务不可用。"));
            if (Result.IsSuccess() && !Released.IsSuccess()) Result = Released;
        }
        return Result;
    }
    /** 操作至多完成一次，先移除回调再调用外部完成，允许同步停止重入。 */
    void Finish(const FGamePlatformResult& Result)
    {
        if (!bWaiting) return;
        bWaiting = false; auto Completion = MoveTemp(PendingCompletion);
        if (Completion) Completion(Context.Token, Result);
    }
    /** 只接受本操作两份真实成功通知；释放A后确认B仍Succeeded且Cube可解析。 */
    void OnSharedLoaded(uint64 Generation, bool bFirst, const FGamePlatformDataLease& Lease, const FGamePlatformResult& Result)
    {
        if (!bPrepared || !bWaiting || Generation != OperationGeneration) return;
        const auto& Expected = bFirst ? A : B;
        if (!Result.IsSuccess() || Lease.ScopeId != Expected.ScopeId || Lease.LeaseId != Expected.LeaseId ||
            Lease.Generation != Expected.Generation || Lease.IssuerProof != Expected.IssuerProof || Lease.DefinitionId != Expected.DefinitionId ||
            Lease.Bundles != Expected.Bundles || Lease.ResourcePaths != Expected.ResourcePaths)
        { Finish(Result.IsSuccess() ? FGamePlatformResult::Failure(FName(TEXT("Demo.DataLeaseMismatch")), TEXT("共享样板完整资源租约不匹配。")) : Result); return; }
        if (bFirst) bAReady = true; else bBReady = true;
        if (!bAReady || !bBReady) return;
        auto* Data = GetData();
        if (!Data) { Finish(FGamePlatformResult::Failure(FName(TEXT("Demo.DataUnavailable")), TEXT("共享验证时中央服务失效。"))); return; }
        const auto Released = Data->ReleaseResources(A);
        if (!Released.IsSuccess()) { Finish(Released); return; }
        const FSoftObjectPath Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
        const bool bShared = Data->GetLeaseState(A) == EGamePlatformDataRequestState::Released &&
            Data->GetLeaseState(B) == EGamePlatformDataRequestState::Succeeded && Cube.ResolveObject() != nullptr;
        Finish(bShared ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(FName(TEXT("Demo.DataSharedLeaseViolation")), TEXT("释放A后B或实际资源未保持可用。")));
    }
};

/** 准备检查实际Data门面；Execute才申请样板资源，Stop/析构撤销本次所有需求。 */
class FDataLeaseScenario final : public IDivineBeastsDemoScenarioAdapter
{
public:
    FDataLeaseScenario() : State(MakeShared<FDataLeaseScenarioState>()) {}
    ~FDataLeaseScenario() override
    {
        State->bPrepared = false; State->bWaiting = false; ++State->OperationGeneration;
        State->PendingCompletion = {}; State->ReleaseLeases();
    }
    void Prepare(const FDivineBeastsDemoScenarioContext& Context, FDivineBeastsDemoOperationCompletion Completion) override
    {
        State->Context = Context; State->Instance = Context.World.IsValid() ? Context.World->GetGameInstance() : nullptr;
        State->bPrepared = State->GetData() != nullptr;
        Completion(Context.Token, State->bPrepared ? FGamePlatformResult::Success() : FGamePlatformResult::Failure(FName(TEXT("Demo.DataUnavailable")), TEXT("样板需要本世界实际中央Data服务。")));
    }
    void Execute(FName Operation, const TArray<FDivineBeastsDemoParameterValue>&, FDivineBeastsDemoOperationCompletion Completion) override
    {
        auto* Data = State->GetData();
        if (!State->bPrepared || !Data || State->bWaiting || !State->Context.World.IsValid())
        { Completion(State->Context.Token, FGamePlatformResult::Failure(FName(TEXT("Demo.DataScenarioNotReady")), TEXT("Data样板未准备、忙碌或世界已退出。"))); return; }
        const auto Released = State->ReleaseLeases();
        if (!Released.IsSuccess() || Operation == TEXT("Reset")) { Completion(State->Context.Token, Released); return; }
        if (Operation != TEXT("Execute"))
        { Completion(State->Context.Token, FGamePlatformResult::Unsupported(FName(TEXT("Demo.DataOperationUnsupported")), TEXT("Data样板仅支持Execute/Reset。"))); return; }
        auto* Caller = State->Context.World->GetSubsystem<UDivineBeastsDemoWorldSubsystem>();
        if (!Caller)
        { Completion(State->Context.Token, FGamePlatformResult::Failure(FName(TEXT("Demo.DataCallerMissing")), TEXT("世界调用者不存在，不能签发World租约。"))); return; }
        State->bWaiting = true; State->bAReady = false; State->bBReady = false;
        State->PendingCompletion = MoveTemp(Completion); const uint64 Generation = ++State->OperationGeneration;
        TWeakPtr<FDataLeaseScenarioState> WeakState(State);
        const TArray<FSoftObjectPath> Resources = {FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube"))};
        FGamePlatformResult Accepted;
        if (State->Context.Scenario.ScenarioId == TEXT("GamePlatformData.Resources.Cancel"))
        {
            State->CancelledLease = Data->AcquireResources(Resources, EGamePlatformDataLifetime::World, Caller,
                [WeakState, Generation](const auto& Lease, const auto& Result)
                {
                    const auto Self = WeakState.Pin();
                    if (!Self || !Self->bPrepared || !Self->bWaiting || Generation != Self->OperationGeneration) return;
                    auto* Service = Self->GetData();
                    const bool bRealCancel = Lease.ScopeId == Self->CancelledLease.ScopeId && Lease.LeaseId == Self->CancelledLease.LeaseId &&
                        Lease.Generation == Self->CancelledLease.Generation && Lease.IssuerProof == Self->CancelledLease.IssuerProof &&
                        Lease.DefinitionId == Self->CancelledLease.DefinitionId && Lease.Bundles == Self->CancelledLease.Bundles &&
                        Lease.ResourcePaths == Self->CancelledLease.ResourcePaths && Result.Status == EGamePlatformResultStatus::Cancelled && Service &&
                        Service->GetLeaseState(Self->CancelledLease) == EGamePlatformDataRequestState::Released;
                    Self->Finish(bRealCancel ? Result : FGamePlatformResult::Failure(FName(TEXT("Demo.DataCancellationMismatch")), TEXT("真实取消通知/完整租约状态不匹配。")));
                }, Accepted);
            if (!Accepted.IsSuccess()) { State->Finish(Accepted); return; }
            const auto Cancelled = Data->ReleaseResources(State->CancelledLease);
            if (!Cancelled.IsSuccess()) State->Finish(Cancelled);
            return;
        }
        if (State->Context.Scenario.ScenarioId != TEXT("GamePlatformData.Resources.SharedLease"))
        { State->Finish(FGamePlatformResult::Unsupported(FName(TEXT("Demo.DataCaseUnsupported")), TEXT("Data样板案例身份未支持。"))); return; }
        State->A = Data->AcquireResources(Resources, EGamePlatformDataLifetime::World, Caller,
            [WeakState, Generation](const auto& Lease, const auto& Result) { if (auto Self = WeakState.Pin()) Self->OnSharedLoaded(Generation, true, Lease, Result); }, Accepted);
        if (!Accepted.IsSuccess()) { State->Finish(Accepted); return; }
        State->B = Data->AcquireResources(Resources, EGamePlatformDataLifetime::World, Caller,
            [WeakState, Generation](const auto& Lease, const auto& Result) { if (auto Self = WeakState.Pin()) Self->OnSharedLoaded(Generation, false, Lease, Result); }, Accepted);
        if (!Accepted.IsSuccess()) State->Finish(Accepted);
    }
    void Stop(FDivineBeastsDemoOperationCompletion Completion) override
    {
        State->bPrepared = false; ++State->OperationGeneration;
        auto Pending = MoveTemp(State->PendingCompletion); State->bWaiting = false;
        const auto Result = State->ReleaseLeases();
        if (Pending) Pending(State->Context.Token, FGamePlatformResult::Cancelled(TEXT("案例停止，在途样板操作取消。")));
        Completion(State->Context.Token, Result);
    }
private:
    TSharedRef<FDataLeaseScenarioState> State;
};
TUniquePtr<IDivineBeastsDemoScenarioAdapter> CreateDataLeaseScenario() { return MakeUnique<FDataLeaseScenario>(); }
}

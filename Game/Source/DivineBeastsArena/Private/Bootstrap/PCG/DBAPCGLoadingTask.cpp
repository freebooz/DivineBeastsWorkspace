#include "Bootstrap/PCG/DBAPCGLoadingTask.h"
#include "Bootstrap/PCG/DBAPCGResultComponent.h"
#include "Bootstrap/PCG/DBAPCGLoadingLifetime.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogDBAPCGLoading, Log, All);

namespace
{
class FDBAPCGLoadingTask final : public IGamePlatformLoadingTask
{
public:
    explicit FDBAPCGLoadingTask(TWeakObjectPtr<UDBAPCGResultComponent> InOwner) : Owner(InOwner) {}
    virtual ~FDBAPCGLoadingTask() override { Release(); }

    FGamePlatformResult Start(UGameInstance& Instance, const FGamePlatformLoadingTaskSpec& Spec) override
    {
        check(IsInGameThread());
        if (bHasStarted || Lifetime.IsReleased())
        { return FGamePlatformResult::Failure(TEXT("PCGTaskAlreadyStarted"), TEXT("PCG加载任务每次尝试必须独占，不允许重复启动。")); }
        bHasStarted = true;
        auto* Component = Owner.Get();
        if (!Component)
        { return FGamePlatformResult::Failure(TEXT("PCGResultOwnerMissing"), TEXT("世界结果组件已失效，工厂不创建全局替身。")); }
        const auto Result = Component->StartGeneration(Instance, Spec.Data.DefinitionId, Handle);
        if (Result.IsSuccess()) { Lifetime.AcceptRequest(); }
        return Result;
    }

    FGamePlatformLoadingTaskUpdate Poll() override
    {
        check(IsInGameThread());
        const auto* Component = Owner.Get();
        if (Lifetime.IsReleased() || !Component || !Handle.IsValid())
        { return Failed(TEXT("PCGTaskContextExpired")); }
        const auto Snapshot = Component->ReadGeneration(Handle);
        if (!(Snapshot.Handle == Handle)) { return Failed(TEXT("PCGTaskHandleMismatch")); }
        if (Snapshot.Result.Status != EGamePlatformResultStatus::NotExecuted && !Snapshot.Result.IsSuccess())
        { return Failed(Snapshot.Result.Code.IsNone() ? FName(TEXT("PCGGenerationFailed")) : Snapshot.Result.Code); }
        if (Snapshot.Outcome == EGamePlatformPCGOutcome::Succeeded &&
            Snapshot.Phase == EGamePlatformPCGPhase::Retained && Snapshot.bIsResultValid && Snapshot.Result.IsSuccess())
        {
            Lifetime.ObserveSuccess();
            return {EGamePlatformLoadingTaskUpdate::Succeeded, 1.0, NAME_None};
        }
        if (Snapshot.Outcome != EGamePlatformPCGOutcome::Pending ||
            Snapshot.Phase == EGamePlatformPCGPhase::Cleaning || Snapshot.Phase == EGamePlatformPCGPhase::Cleaned)
        { return Failed(TEXT("PCGOutputNotRetained")); }
        return {};
    }

    bool IsReadyToUse() const override
    {
        check(IsInGameThread());
        const auto* Component = Owner.Get();
        if (!Component || Lifetime.IsReleased() || !Lifetime.HasSucceeded()) { return false; }
        const auto Snapshot = Component->ReadGeneration(Handle);
        return Snapshot.Handle == Handle && Snapshot.Outcome == EGamePlatformPCGOutcome::Succeeded &&
            Snapshot.Phase == EGamePlatformPCGPhase::Retained && Snapshot.bIsResultValid && Snapshot.Result.IsSuccess();
    }

    void Release() override
    {
        check(IsInGameThread());
        if (Lifetime.ReleaseTask())
        {
            if (auto* Component = Owner.Get())
            {
                const auto Result = Component->ReleaseGeneration(Handle);
                if (!Result.IsSuccess())
                {
                    UE_LOG(LogDBAPCGLoading, Warning, TEXT("PCG task release rejected Code=%s; world component retains exact handle for retry"), *Result.Code.ToString());
                }
            }
        }
        Owner.Reset();
    }
private:
    static FGamePlatformLoadingTaskUpdate Failed(FName Code)
    { return {EGamePlatformLoadingTaskUpdate::Failed, 0.0, Code}; }

    TWeakObjectPtr<UDBAPCGResultComponent> Owner;
    FGamePlatformPCGHandle Handle;
    DBA::PCG::FLoadingLifetime Lifetime;
    bool bHasStarted = false;
};
}

FGamePlatformLoadingTaskFactory DBAPCGLoading::CreateTaskFactory(UDBAPCGResultComponent& ResultOwner)
{
    check(IsInGameThread());
    return [WeakOwner = TWeakObjectPtr<UDBAPCGResultComponent>(&ResultOwner)]() -> TUniquePtr<IGamePlatformLoadingTask>
    {
        return MakeUnique<FDBAPCGLoadingTask>(WeakOwner);
    };
}

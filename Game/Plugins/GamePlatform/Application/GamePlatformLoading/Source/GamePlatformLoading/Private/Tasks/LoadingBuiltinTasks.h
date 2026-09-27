#pragma once
#include "Interfaces/IGamePlatformLoadingTask.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

/** 操作独占的基础世界声明；弱引用不延长旧世界生命周期。 */
struct FLoadingWorldEvidence
{
    TWeakObjectPtr<UGameInstance> Instance;
    TWeakObjectPtr<UWorld> OperableWorld;
    FString Package;
    bool IsValid() const
    {
        const auto* World = OperableWorld.Get();
        return Instance.IsValid() && World && Instance->GetWorld() == World &&
            World->GetGameInstance() == Instance.Get() && !World->bIsTearingDown && World->HasBegunPlay() &&
            (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE) &&
            UWorld::RemovePIEPrefix(World->GetOutermost()->GetName()) == Package;
    }
};

/** 真实Data门面适配；只持有本任务的租约，不触碰主资产管理器。 */
class FLoadingDataTask final : public IGamePlatformLoadingTask
{
public:
    FGamePlatformResult Start(UGameInstance& InInstance, const FGamePlatformLoadingTaskSpec& Spec) override
    {
        Instance = &InInstance;
        auto* Data = IGamePlatformDataService::Get(InInstance);
        if (!Data) { return FGamePlatformResult::Failure(TEXT("DataUnavailable"), TEXT("本实例数据服务不可用")); }
        Completion = MakeShared<FGamePlatformResult>();
        FGamePlatformResult Accepted;
        Lease = Data->AcquireDefinition(Spec.Data.DefinitionId, Spec.Data.ExpectedClass, Spec.Data.Bundles,
            EGamePlatformDataLifetime::Instance, &InInstance,
            [Result = Completion](const FGamePlatformDataLease&, const FGamePlatformResult& Value) { *Result = Value; }, Accepted);
        return Accepted;
    }
    FGamePlatformLoadingTaskUpdate Poll() override
    {
        if (!Instance.IsValid()) { return {EGamePlatformLoadingTaskUpdate::Failed,0,TEXT("InstanceExpired")}; }
        auto* Data = IGamePlatformDataService::Get(*Instance.Get());
        if (!Data) { return {EGamePlatformLoadingTaskUpdate::Failed,0,TEXT("DataUnavailable")}; }
        const auto State = Data->GetLeaseState(Lease);
        if (State == EGamePlatformDataRequestState::Succeeded && Data->GetLoadedDefinition(Lease))
        { return {EGamePlatformLoadingTaskUpdate::Succeeded,1,NAME_None}; }
        if (State == EGamePlatformDataRequestState::Loading) { return {}; }
        return {EGamePlatformLoadingTaskUpdate::Failed,0,
            Completion.IsValid() && !Completion->Code.IsNone() ? Completion->Code : FName(TEXT("DataLeaseInvalid"))};
    }
    void Release() override
    {
        if (Instance.IsValid() && Lease.IsValid())
        { if (auto* Data = IGamePlatformDataService::Get(*Instance.Get())) { Data->ReleaseDefinition(Lease); } }
        Lease = {}; Completion.Reset(); Instance.Reset();
    }
private:
    TWeakObjectPtr<UGameInstance> Instance;
    FGamePlatformDataLease Lease;
    TSharedPtr<FGamePlatformResult> Completion;
};

/** 基础世界屏障，只有组合根的精确操作声明和当前世界事实同时成立才能完成。 */
class FLoadingWorldTask final : public IGamePlatformLoadingTask
{
public:
    explicit FLoadingWorldTask(TSharedRef<FLoadingWorldEvidence> InEvidence) : Evidence(InEvidence) {}
    FGamePlatformResult Start(UGameInstance&, const FGamePlatformLoadingTaskSpec&) override
    { return Evidence->Package.IsEmpty() ? FGamePlatformResult::Failure(TEXT("TargetWorldMissing"),TEXT("世界任务需要目标包身份")) : FGamePlatformResult::Success(); }
    FGamePlatformLoadingTaskUpdate Poll() override
    { return Evidence->IsValid() ? FGamePlatformLoadingTaskUpdate{EGamePlatformLoadingTaskUpdate::Succeeded,1,NAME_None} : FGamePlatformLoadingTaskUpdate{}; }
    // 声明归整个操作持有；单个任务释放不撤销其他任务引用的同一声明。
    void Release() override { Evidence->OperableWorld.Reset(); }
private:
    TSharedRef<FLoadingWorldEvidence> Evidence;
};

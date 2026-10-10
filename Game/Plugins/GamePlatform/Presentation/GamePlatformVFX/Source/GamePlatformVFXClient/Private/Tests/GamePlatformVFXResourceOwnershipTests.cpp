// 本文件属于GamePlatform平台层 GamePlatformVFX，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// F05/F08：等待资源取消、旧回执、失败、世界退出与终态一次；夹具不伪造资源成功/真实Niagara资产。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "NiagaraComponent.h"
#include "Definitions/GamePlatformVFXInstantDefinition.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Settings/GamePlatformVFXSettings.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXResourceOwnershipTest,
    "GamePlatform.VFX.Lifecycle.ResourceCancellationAndTerminalOnce", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXResourceOwnershipTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> Service(NewObject<UGamePlatformVFXWorldSubsystem>(World.Get()));
    const auto Handle = Service->InstanceRegistry.Reserve(World.Get());
    FGamePlatformVFXPlaybackSnapshot Snapshot; Snapshot.Handle = Handle; Snapshot.State = EGamePlatformVFXPlaybackState::Loading;
    Service->PlaybackSnapshots.Add(Handle.Id, Snapshot);
    UGamePlatformVFXWorldSubsystem::FInstanceResourceRequest Pending; Pending.Handle = Handle; Pending.bLoading = true;
    Pending.Lease.LeaseId = FGuid::NewGuid(); Pending.Lease.Generation = 2;
    const auto Lease = Pending.Lease; Service->InstanceResources.Add(Handle.Id, Pending); Service->PendingInstanceCount = 1;
    int32 Completions = 0; Service->AddCompletionHandler(FGamePlatformVFXPlaybackCompleted::FDelegate::CreateLambda(
        [&](const FGamePlatformVFXPlaybackSnapshot&) { ++Completions; }));
    auto Forged = Handle; ++Forged.Generation;
    TestFalse(TEXT("修改代次不能取消真实实例"), Service->Stop(Forged));
    TestTrue(TEXT("等待资源的完整句柄可取消"), Service->Stop(Handle));
    TestEqual(TEXT("取消释放待加载容量"), Service->PendingInstanceCount, 0);
    TestFalse(TEXT("取消清理自有资源账本"), Service->InstanceResources.Contains(Handle.Id));
    Service->HandleSelectedResourcesLoaded(Handle, Lease, FGamePlatformResult::Success());
    TestEqual(TEXT("旧资源完成不能复活且只通知一次"), Completions, 1);
    TestEqual(TEXT("可查询明确取消终态"), Service->GetPlaybackSnapshot(Handle).State, EGamePlatformVFXPlaybackState::Cancelled);
    const auto Failed = Service->InstanceRegistry.Reserve(World.Get());
    Service->CleanupInstance(Failed, true, EGamePlatformVFXPlaybackState::Failed, EGamePlatformVFXResultCode::DefinitionLoadFailed, TEXT("测试缺配置失败"));
    TestEqual(TEXT("失败区别于自然完成"), Service->GetPlaybackSnapshot(Failed).State, EGamePlatformVFXPlaybackState::Failed);
    TestFalse(TEXT("失败原因可诊断"), Service->GetPlaybackSnapshot(Failed).Diagnostic.IsEmpty());
    // 复核补充：按真实Niagara先失活后Finished的顺序驱动生产回调；不创建Niagara System资产。
    const TStrongObjectPtr<UNiagaraComponent> FinishedComponent(NewObject<UNiagaraComponent>(World.Get()));
    const TStrongObjectPtr<UGamePlatformVFXInstantDefinition> Definition(NewObject<UGamePlatformVFXInstantDefinition>());
    const auto Natural = Service->InstanceRegistry.Reserve(World.Get());
    Service->InstanceRegistry.SetDefinition(Natural, Definition.Get());
    Service->InstanceRegistry.AttachComponent(Natural, FinishedComponent.Get(), false);
    const FName DefinitionId(TEXT("presentation.test.natural@1"));
    auto& Cached = Service->DefinitionCache.FindOrAdd(DefinitionId); Cached.Definition = Definition.Get(); Cached.ActiveUsers = 1;
    Service->DefinitionIdByHandle.Add(Natural.Id, DefinitionId);
    Snapshot.Handle = Natural; Snapshot.State = EGamePlatformVFXPlaybackState::Playing;
    Service->PlaybackSnapshots.Add(Natural.Id, Snapshot);
    FGamePlatformVFXRequest NaturalRequest; NaturalRequest.RequestId = FGuid::NewGuid();
    const auto NaturalKey = Service->MakeDedupeKey(NaturalRequest); Service->AddDedupeHandle(NaturalKey, Natural);
    UGamePlatformVFXWorldSubsystem::FInstanceResourceRequest ResourceOwnership;
    ResourceOwnership.Handle = Natural; Service->InstanceResources.Add(Natural.Id, ResourceOwnership);
    FinishedComponent->OnSystemFinished.AddUniqueDynamic(Service.Get(), &UGamePlatformVFXWorldSubsystem::HandleSystemFinished);
    FinishedComponent->SetActiveFlag(false);
    TestFalse(TEXT("真实完成通知前组件已不活跃"), Service->IsActive(Natural));
    const int32 NotificationsBeforeNatural = Completions;
    FinishedComponent->OnSystemFinished.Broadcast(FinishedComponent.Get());
    TestEqual(TEXT("自然完成发出一次终态通知"), Completions, NotificationsBeforeNatural + 1);
    TestEqual(TEXT("自然完成回收实例总数"), Service->InstanceRegistry.Num(), 0);
    TestEqual(TEXT("自然完成释放定义使用计数"), Service->DefinitionCache.FindChecked(DefinitionId).ActiveUsers, 0);
    TestFalse(TEXT("自然完成清除定义索引"), Service->DefinitionIdByHandle.Contains(Natural.Id));
    TestFalse(TEXT("自然完成清除资源所有权"), Service->InstanceResources.Contains(Natural.Id));
    TestFalse(TEXT("自然完成清除去重所有权"), Service->DedupeHandles.Contains(NaturalKey));
    TestEqual(TEXT("自然完成留下完成终态"), Service->GetPlaybackSnapshot(Natural).State, EGamePlatformVFXPlaybackState::Completed);
    const int32 NotificationsAfterNatural = Completions;
    FinishedComponent->OnSystemFinished.Broadcast(FinishedComponent.Get());
    TestEqual(TEXT("重复自然完成只通知一次"), Completions, NotificationsAfterNatural);
    const auto InactiveOwned = Service->InstanceRegistry.Reserve(World.Get());
    Service->InstanceRegistry.AttachComponent(InactiveOwned, FinishedComponent.Get(), false);
    TestTrue(TEXT("不活跃但仍有账本的实例允许显式Stop"), Service->Stop(InactiveOwned));
    TestEqual(TEXT("不活跃Stop清除实例"), Service->InstanceRegistry.Num(), 0);
    const TStrongObjectPtr<UGamePlatformVFXCompositeDefinition> Composite(NewObject<UGamePlatformVFXCompositeDefinition>());
    FGamePlatformVFXCompositeStep Step; Step.DefinitionId = TEXT("presentation.test.required.child@1"); Composite->Steps.Add(Step);
    auto CompositeHandle = Service->InstanceRegistry.Reserve(World.Get());
    FGamePlatformVFXRequest CompositeRequest; CompositeRequest.CompositeDepth = Composite->MaxDepth;
    TestFalse(TEXT("达到Composite深度边界未执行必需子，不能标Playing"), Service->ExecuteLoadedDefinition(*Composite, CompositeRequest, CompositeHandle));
    TestEqual(TEXT("深度边界形成失败终态"), Service->GetPlaybackSnapshot(CompositeHandle).State, EGamePlatformVFXPlaybackState::Failed);
    TestEqual(TEXT("深度拒绝整树账本为零"), Service->InstanceRegistry.Num(), 0);
    {
        TGuardValue<int32> TestHardLimit(GetMutableDefault<UGamePlatformVFXSettings>()->HardMaxTrackedInstances, 1);
        CompositeHandle = Service->InstanceRegistry.Reserve(World.Get()); CompositeRequest.CompositeDepth = 0;
        TestFalse(TEXT("必需子容量拒绝不能静默让父Playing"), Service->ExecuteLoadedDefinition(*Composite, CompositeRequest, CompositeHandle));
        TestEqual(TEXT("容量拒绝保留真实原因"), Service->GetPlaybackSnapshot(CompositeHandle).Code, EGamePlatformVFXResultCode::RejectedByScalability);
        TestEqual(TEXT("容量拒绝撤销整树"), Service->InstanceRegistry.Num(), 0);
    }
    UGamePlatformVFXWorldSubsystem::FPreloadRecord Preload;
    Preload.Handle.Id = FGuid::NewGuid(); Preload.Handle.Generation = 3; Preload.Handle.World = World.Get(); Preload.PendingLoads = 1;
    const auto PreloadHandle = Preload.Handle; Service->PreloadRecords.Add(PreloadHandle.Id, Preload);
    FString PreloadError;
    TestEqual(TEXT("预载没有资源完成时不得Ready"), Service->GetPreloadState(PreloadHandle, PreloadError), EGamePlatformVFXPreloadState::Loading);
    TestTrue(TEXT("预载取消只撤销本事务"), Service->CancelPreload(PreloadHandle));
    Service->HandlePreloadResourcesLoaded(PreloadHandle, Lease, FGamePlatformResult::Success());
    TestEqual(TEXT("预载旧成功不能取消后Ready"), Service->GetPreloadState(PreloadHandle, PreloadError), EGamePlatformVFXPreloadState::Cancelled);
    const auto Closing = Service->InstanceRegistry.Reserve(World.Get());
    const TStrongObjectPtr<UNiagaraComponent> InactiveAtExit(NewObject<UNiagaraComponent>(World.Get()));
    Service->InstanceRegistry.AttachComponent(Closing, InactiveAtExit.Get(), false); InactiveAtExit->SetActiveFlag(false);
    Snapshot.Handle = Closing; Service->PlaybackSnapshots.Add(Closing.Id, Snapshot);
    Service->Deinitialize();
    TestEqual(TEXT("世界退出形成独立终态"), Service->GetPlaybackSnapshot(Closing).State, EGamePlatformVFXPlaybackState::WorldDestroyed);
    TestEqual(TEXT("世界退出清除不活跃但owned实例"), Service->InstanceRegistry.Num(), 0);
    const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> CorrectedService(NewObject<UGamePlatformVFXWorldSubsystem>(World.Get()));
    FGamePlatformVFXRequest Correction; Correction.RequestId = FGuid::NewGuid(); Correction.DefinitionId = TEXT("presentation.test.corrected@1");
    Correction.PredictionState = EGamePlatformVFXPredictionState::Corrected;
    const auto BeforeCorrection = CorrectedService->InstanceRegistry.Reserve(World.Get());
    CorrectedService->AddDedupeHandle(CorrectedService->MakeDedupeKey(Correction), BeforeCorrection);
    Snapshot.Handle = BeforeCorrection; Snapshot.State = EGamePlatformVFXPlaybackState::Loading;
    CorrectedService->PlaybackSnapshots.Add(BeforeCorrection.Id, Snapshot);
    CorrectedService->AddCompletionHandler(FGamePlatformVFXPlaybackCompleted::FDelegate::CreateLambda(
        [&](const FGamePlatformVFXPlaybackSnapshot&) { CorrectedService->Deinitialize(); }));
    TestEqual(TEXT("纠正Stop通知关闭服务后拒绝替换"), CorrectedService->Play(Correction).Code, EGamePlatformVFXResultCode::InvalidWorld);
    TestEqual(TEXT("关闭后未重新Reserve实例"), CorrectedService->InstanceRegistry.Num(), 0);
    TestEqual(TEXT("关闭后未重新申请定义账本"), CorrectedService->DefinitionCache.Num(), 0);
    World->DestroyWorld(false);
    return true;
}
#endif

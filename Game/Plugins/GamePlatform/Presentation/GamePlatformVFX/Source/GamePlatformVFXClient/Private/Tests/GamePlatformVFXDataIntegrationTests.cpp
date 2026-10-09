// 本文件属于平台客户端VFX跨模块回归；使用公开Data真实服务，抽象IsA约束与已加载UE类资源，不伪造Definition/Niagara资产。
// 缺定义必须失败，自然结束必须释放真实租约；中文前置/失败/生命周期与未执行边界见Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Definitions/GamePlatformVFXDefinition.h"
#include "Definitions/GamePlatformVFXInstantDefinition.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Engine/AssetManager.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "HAL/PlatformTime.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/** 只保留本用例的真实服务/资源身份和完成值；不替换资产管理器、不绕过Data校验。 */
struct FVFXDataIntegrationState
{
    TStrongObjectPtr<UGameInstance> GameInstance;
    TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> Service;
    FGamePlatformDataLease DefinitionLease;
    FGamePlatformDataLease ResourceLease;
    FGamePlatformVFXHandle PlaybackHandle;
    FGamePlatformVFXPreloadHandle PreloadHandle;
    FGamePlatformResult DefinitionResult;
    FGamePlatformResult ResourceResult;
    int32 Completions = 0;
    double StartedAtSeconds = FPlatformTime::Seconds();
};
/** 等实际异步回执；5秒只为测试超时，超时仍清理本调用者需求且使测试失败。 */
class FVFXDataIntegrationWait final : public IAutomationLatentCommand
{
public:
    FVFXDataIntegrationWait(TSharedRef<FVFXDataIntegrationState> InState, TFunction<void()> InVerify)
        : State(MoveTemp(InState)), Verify(MoveTemp(InVerify)) {}
    virtual bool Update() override
    {
        FString PreloadError;
        const bool bStillLoading = State->Completions < 2 ||
            State->Service->GetPlaybackSnapshot(State->PlaybackHandle).State == EGamePlatformVFXPlaybackState::Loading ||
            State->Service->GetPreloadState(State->PreloadHandle, PreloadError) == EGamePlatformVFXPreloadState::Loading;
        if (bStillLoading && FPlatformTime::Seconds() - State->StartedAtSeconds < 5.0) return false;
        Verify(); return true;
    }
private:
    TSharedRef<FVFXDataIntegrationState> State;
    TFunction<void()> Verify;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXDataIntegrationTest,
    "GamePlatform.VFX.Data.AbstractConstraintMissingDefinitionAndNaturalLeaseRelease",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXDataIntegrationTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实引擎已启动"), GEngine)) return false;
    auto State = MakeShared<FVFXDataIntegrationState>();
    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine)); State->GameInstance->Init();
    State->World.Reset(UWorld::CreateWorld(EWorldType::Game, false)); State->World->SetGameInstance(State->GameInstance.Get());
    State->Service.Reset(NewObject<UGamePlatformVFXWorldSubsystem>(State->World.Get()));
    auto* Data = IGamePlatformDataService::Get(*State->GameInstance);
    if (!TestNotNull(TEXT("初始化实际GameInstance得到真实Data公开服务"), Data))
    { State->GameInstance->Shutdown(); State->World->DestroyWorld(false); return false; }
    const FPrimaryAssetId MissingId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), TEXT("presentation.remediation.abstract.base.missing@1"));
    TestTrue(TEXT("VFX服务实际请求的ExpectedClass是抽象基类"), UGamePlatformVFXDefinition::StaticClass()->HasAnyClassFlags(CLASS_Abstract));
    TestFalse(TEXT("前置明确无实际注册定义，不提供伪成功资源"), UAssetManager::Get().GetPrimaryAssetPath(MissingId).IsValid());
    const TWeakPtr<FVFXDataIntegrationState> WeakState(State);
    FGamePlatformResult Accepted;
    State->DefinitionLease = Data->AcquireDefinition(MissingId, UGamePlatformVFXDefinition::StaticClass(), {},
        EGamePlatformDataLifetime::Instance, State->GameInstance.Get(),
        [WeakState](const FGamePlatformDataLease&, const FGamePlatformResult& Result)
        { if (auto Pinned = WeakState.Pin()) { Pinned->DefinitionResult = Result; ++Pinned->Completions; } }, Accepted);
    TestTrue(TEXT("抽象类型只用于IsA约束，实际Data允许受理"), Accepted.IsSuccess() && State->DefinitionLease.IsValid());
    // 真实已加载NiagaraComponent UClass是普通软资源；这不等于存在可播放Niagara System。
    State->ResourceLease = Data->AcquireResources({ FSoftObjectPath(UNiagaraComponent::StaticClass()->GetPathName()) },
        EGamePlatformDataLifetime::World, State->Service.Get(),
        [WeakState](const FGamePlatformDataLease&, const FGamePlatformResult& Result)
        { if (auto Pinned = WeakState.Pin()) { Pinned->ResourceResult = Result; ++Pinned->Completions; } }, Accepted);
    TestTrue(TEXT("真实已加载类资源签发Data租约"), Accepted.IsSuccess() && State->ResourceLease.IsValid());
    FGamePlatformVFXRequest MissingRequest; MissingRequest.DefinitionId = MissingId.PrimaryAssetName; MissingRequest.RequestId = FGuid::NewGuid();
    const auto Playback = State->Service->Play(MissingRequest); State->PlaybackHandle = Playback.Handle;
    TestEqual(TEXT("实际VFX Play使用抽象约束后由Data接纳，受理不冒充播放"), Playback.Code, EGamePlatformVFXResultCode::Queued);
    State->PreloadHandle = State->Service->Preload(MissingRequest);
    TestTrue(TEXT("实际VFX Preload允许抽象约束受理"), State->PreloadHandle.IsValid());
    ADD_LATENT_AUTOMATION_COMMAND(FVFXDataIntegrationWait(State, [this, State]()
    {
        auto* ActualData = IGamePlatformDataService::Get(*State->GameInstance);
        TestEqual(TEXT("两个实际完成回执均到达一次"), State->Completions, 2);
        TestFalse(TEXT("缺注册定义仍实际加载失败，不能把受理当成功"), State->DefinitionResult.IsSuccess());
        TestEqual(TEXT("实际VFX Play缺定义终态失败"), State->Service->GetPlaybackSnapshot(State->PlaybackHandle).State, EGamePlatformVFXPlaybackState::Failed);
        FString PreloadError;
        TestEqual(TEXT("实际VFX Preload缺定义终态失败"), State->Service->GetPreloadState(State->PreloadHandle, PreloadError), EGamePlatformVFXPreloadState::Failed);
        TestFalse(TEXT("实际Preload缺定义保留原因"), PreloadError.IsEmpty());
        if (ActualData)
        {
            TestEqual(TEXT("缺定义形成Data Failed终态"), ActualData->GetLeaseState(State->DefinitionLease), EGamePlatformDataRequestState::Failed);
            TestNull(TEXT("缺定义无只读对象"), ActualData->GetLoadedDefinition(State->DefinitionLease));
            TestTrue(TEXT("UE类软资源真实加载完成"), State->ResourceResult.IsSuccess());
            if (State->ResourceResult.IsSuccess())
            {
                auto* Service = State->Service.Get();
                const TStrongObjectPtr<UNiagaraComponent> Component(NewObject<UNiagaraComponent>(State->World.Get()));
                const TStrongObjectPtr<UGamePlatformVFXInstantDefinition> Metadata(NewObject<UGamePlatformVFXInstantDefinition>());
                const auto Handle = Service->InstanceRegistry.Reserve(State->World.Get());
                Service->InstanceRegistry.SetDefinition(Handle, Metadata.Get()); Service->InstanceRegistry.AttachComponent(Handle, Component.Get(), false);
                const FName MetadataId(TEXT("presentation.test.natural.owned@1"));
                auto& Cached = Service->DefinitionCache.FindOrAdd(MetadataId); Cached.Definition = Metadata.Get(); Cached.ActiveUsers = 1;
                Service->DefinitionIdByHandle.Add(Handle.Id, MetadataId);
                UGamePlatformVFXWorldSubsystem::FInstanceResourceRequest Resources; Resources.Handle = Handle; Resources.Lease = State->ResourceLease;
                Service->InstanceResources.Add(Handle.Id, Resources);
                FGamePlatformVFXPlaybackSnapshot Snapshot; Snapshot.Handle = Handle; Snapshot.State = EGamePlatformVFXPlaybackState::Playing;
                Service->PlaybackSnapshots.Add(Handle.Id, Snapshot);
                int32 Notifications = 0; Service->AddCompletionHandler(FGamePlatformVFXPlaybackCompleted::FDelegate::CreateLambda(
                    [&](const FGamePlatformVFXPlaybackSnapshot&) { ++Notifications; }));
                Component->OnSystemFinished.AddUniqueDynamic(Service, &UGamePlatformVFXWorldSubsystem::HandleSystemFinished);
                Component->SetActiveFlag(false); Component->OnSystemFinished.Broadcast(Component.Get());
                TestEqual(TEXT("自然完成实际释放本服务Data资源租约"), ActualData->GetLeaseState(State->ResourceLease), EGamePlatformDataRequestState::Released);
                TestEqual(TEXT("自然完成实例账本为零"), Service->InstanceRegistry.Num(), 0);
                TestEqual(TEXT("自然完成定义使用计数为零"), Service->DefinitionCache.FindChecked(MetadataId).ActiveUsers, 0);
                TestEqual(TEXT("自然完成资源账本为零"), Service->InstanceResources.Num(), 0);
                TestEqual(TEXT("自然完成仅一个完成通知"), Notifications, 1);
                TestEqual(TEXT("自然完成保留Completed快照"), Service->GetPlaybackSnapshot(Handle).State, EGamePlatformVFXPlaybackState::Completed);
                Component->OnSystemFinished.Broadcast(Component.Get()); TestEqual(TEXT("重复完成不能重复通知"), Notifications, 1);
                Service->PlaybackCompleted.Clear();
            }
            ActualData->ReleaseResources(State->ResourceLease); ActualData->ReleaseDefinition(State->DefinitionLease);
            TestEqual(TEXT("真实Data请求释放后待加载为零"), ActualData->GetDiagnostics().PendingRequests, 0);
            TestEqual(TEXT("真实Data请求释放后活跃租约为零"), ActualData->GetDiagnostics().ActiveLeases, 0);
            // 真实Data账本核对纠正Stop通知关停后不申请替换租约，不用计数替身冒充资源执行。
            auto* Service = State->Service.Get();
            FGamePlatformVFXRequest Correction; Correction.DefinitionId = TEXT("presentation.remediation.abstract.base.missing@1");
            Correction.RequestId = FGuid::NewGuid(); Correction.PredictionState = EGamePlatformVFXPredictionState::Corrected;
            const auto OldHandle = Service->InstanceRegistry.Reserve(State->World.Get());
            Service->AddDedupeHandle(Service->MakeDedupeKey(Correction), OldHandle);
            const int64 AcceptedBeforeCorrection = ActualData->GetDiagnostics().TotalAcceptedRequests;
            Service->AddCompletionHandler(FGamePlatformVFXPlaybackCompleted::FDelegate::CreateLambda(
                [Service](const FGamePlatformVFXPlaybackSnapshot&) { Service->Deinitialize(); }));
            TestEqual(TEXT("真实Data环境纠正Stop关停后拒绝替换"), Service->Play(Correction).Code, EGamePlatformVFXResultCode::InvalidWorld);
            TestEqual(TEXT("真实Data接纳总数在关停后不增加"), ActualData->GetDiagnostics().TotalAcceptedRequests, AcceptedBeforeCorrection);
            TestEqual(TEXT("真实Data关停后没有新Pending"), ActualData->GetDiagnostics().PendingRequests, 0);
        }
        State->Service->Deinitialize(); State->GameInstance->Shutdown(); State->World->DestroyWorld(false);
    }));
    return true;
}
#endif

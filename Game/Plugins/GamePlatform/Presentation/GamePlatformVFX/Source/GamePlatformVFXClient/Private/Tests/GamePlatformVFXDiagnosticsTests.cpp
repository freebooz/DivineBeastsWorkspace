// 跨世界诊断回归：覆盖引擎世界销毁及生产资源状态转换；不证明Niagara资产或视觉质量。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Diagnostics/GamePlatformVFXDiagnostics.h"
#include "Diagnostics/GamePlatformVFXRuntimeCountRegistry.h"
#include "Definitions/GamePlatformPrimaryDataAsset.h"
#include "Definitions/GamePlatformVFXCompositeDefinition.h"
#include "Interfaces/IGamePlatformDataService.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Settings/GamePlatformVFXSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "NiagaraComponent.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
/** 仅本测试拥有的世界。普通Editor复用引擎实例并由DestroyWorld注销；强引用保留到迟到回调断言后。 */
struct FVFXDiagnosticsWorldFixture
{
    TStrongObjectPtr<UWorld> World;
    TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> Service;
    FSubsystemCollection<UGamePlatformVFXWorldSubsystem> ManualCollection;
    bool bManualService = false;
    bool bDestroyed = false;

    bool Initialize(UGameInstance* GameInstance = nullptr)
    {
        // 只在构造期间关闭旧Catalog预载；不让外部资源配置影响计数夹具，也不跨异步帧改CDO。
        TGuardValue<TArray<TSoftObjectPtr<UGamePlatformVFXCatalog>>> EmptyCatalogs(
            GetMutableDefault<UGamePlatformVFXSettings>()->StartupCatalogs, {});
        World.Reset(UWorld::CreateWorld(EWorldType::Game, false));
        if (!World.IsValid()) return false;
        if (GameInstance) World->SetGameInstance(GameInstance);
        Service.Reset(World->GetSubsystem<UGamePlatformVFXWorldSubsystem>());
        if (!Service.IsValid() && IsRunningCommandlet())
        {
            // 生产门禁在Commandlet不创建服务；此分支只验证显式生命周期，不冒充引擎自动创建。
            bManualService = true;
            Service.Reset(NewObject<UGamePlatformVFXWorldSubsystem>(World.Get()));
            Service->Initialize(ManualCollection);
        }
        return Service.IsValid();
    }

    void Destroy()
    {
        if (bDestroyed || !World.IsValid()) return;
        bDestroyed = true;
        if (bManualService && Service.IsValid()) Service->Deinitialize();
        World->DestroyWorld(false);
    }

    ~FVFXDiagnosticsWorldFixture() { Destroy(); }
};

/** 复用现有Data集成测试的真实服务及UE类资源；缓存元数据仅属于夹具，不注册虚构的生产资产。 */
struct FVFXDiagnosticsResourceState
{
    TStrongObjectPtr<UGameInstance> GameInstance;
    FVFXDiagnosticsWorldFixture Fixture;
    TStrongObjectPtr<UGamePlatformVFXCompositeDefinition> Definition;
    FGamePlatformVFXRuntimeCountSnapshot Before;
    FGamePlatformVFXHandle Handle;
    FGamePlatformDataLease ResourceLease;
    double StartedAtSeconds = 0.0;

    ~FVFXDiagnosticsResourceState()
    {
        Fixture.Destroy();
        if (GameInstance.IsValid()) GameInstance->Shutdown();
    }
};

/** 等待真实Data异步回执，最多5秒；超时仍执行断言及夹具清理，不将受理当作完成。 */
class FVFXDiagnosticsResourceWait final : public IAutomationLatentCommand
{
public:
    FVFXDiagnosticsResourceWait(TSharedRef<FVFXDiagnosticsResourceState> InState, TFunction<void()> InVerify)
        : State(MoveTemp(InState)), Verify(MoveTemp(InVerify)) {}

    virtual bool Update() override
    {
        const bool bLoading = State->Fixture.Service->GetPlaybackSnapshot(State->Handle).State == EGamePlatformVFXPlaybackState::Loading;
        if (bLoading && FPlatformTime::Seconds() - State->StartedAtSeconds < 5.0) return false;
        Verify();
        return true;
    }

private:
    TSharedRef<FVFXDiagnosticsResourceState> State;
    TFunction<void()> Verify;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXRuntimeCountRegistryTest,
    "GamePlatform.VFX.Diagnostics.MultiWorldCounts", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXRuntimeCountRegistryTest::RunTest(const FString&)
{
    FGamePlatformVFXRuntimeCountRegistry Registry;
    const FGuid A = Registry.RegisterWorld();
    const FGuid B = Registry.RegisterWorld();
    Registry.UpdateWorld(A, 4, 1);
    Registry.UpdateWorld(B, 3, 2);
    TestEqual(TEXT("两个世界的实例求和"), Registry.GetSnapshot().TrackedInstances, 7);
    TestEqual(TEXT("等待实例也跨世界求和"), Registry.GetSnapshot().PendingInstances, 3);
    Registry.UpdateWorld(A, 1, 0);
    TestEqual(TEXT("更新一个世界保留另一个世界"), Registry.GetSnapshot().TrackedInstances, 4);
    TestEqual(TEXT("峰值记录曾同时存在的总数"), Registry.GetSnapshot().PeakTrackedInstances, 7);
    Registry.UnregisterWorld(A);
    TestEqual(TEXT("一个世界退出后保留另一个世界计数"), Registry.GetSnapshot().TrackedInstances, 3);
    TestFalse(TEXT("退出世界的迟到更新不能复活"), Registry.UpdateWorld(A, 100, 100));
    TestFalse(TEXT("重复注销无副作用"), Registry.UnregisterWorld(A));
    Registry.UnregisterWorld(B);
    TestEqual(TEXT("最后一个世界退出清空当前计数"), Registry.GetSnapshot().TrackedInstances, 0);
    TestEqual(TEXT("世界退出不清除模块历史峰值"), Registry.GetSnapshot().PeakTrackedInstances, 7);
    const FGuid C = Registry.RegisterWorld();
    Registry.UpdateWorld(C, -1, -2);
    TestEqual(TEXT("负输入不会导致无符号计数溢出"), Registry.GetSnapshot().PendingInstances, 0);
    const FGuid D = Registry.RegisterWorld();
    Registry.UpdateWorld(C, MAX_int32, MAX_int32);
    Registry.UpdateWorld(D, 3, 2);
    TestEqual(TEXT("多世界极端计数饱和"), Registry.GetSnapshot().TrackedInstances, MAX_int32);
    Registry.UnregisterWorld(C);
    TestEqual(TEXT("饱和后注销仍精确保留其他世界"), Registry.GetSnapshot().TrackedInstances, 3);
    Registry.Reset();
    TestEqual(TEXT("模块结束清除注册世界"), Registry.GetSnapshot().RegisteredWorlds, 0);
    TestEqual(TEXT("模块结束重置峰值"), Registry.GetSnapshot().PeakTrackedInstances, 0);
    TestFalse(TEXT("模块结束后的旧句柄无效"), Registry.UpdateWorld(D, 5, 5));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXDiagnosticsWorldLifecycleTest,
    "GamePlatform.VFX.Diagnostics.WorldSubsystemLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXDiagnosticsWorldLifecycleTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实引擎已启动"), GEngine)) return false;
    const auto Before = FGamePlatformVFXDiagnostics::GetRuntimeCounts();
    FVFXDiagnosticsWorldFixture WorldA;
    FVFXDiagnosticsWorldFixture WorldB;
    if (!TestTrue(TEXT("A世界取得一次初始化的VFX服务"), WorldA.Initialize()) ||
        !TestTrue(TEXT("B世界取得一次初始化的VFX服务"), WorldB.Initialize())) return false;
    auto* A = WorldA.Service.Get();
    auto* B = WorldB.Service.Get();
    if (IsRunningCommandlet()) AddInfo(TEXT("Commandlet只覆盖显式服务生命周期；普通Editor路径才验证引擎自动创建/销毁。"));
    else
    {
        TestFalse(TEXT("普通Editor的A服务由引擎管理"), WorldA.bManualService);
        TestFalse(TEXT("普通Editor的B服务由引擎管理"), WorldB.bManualService);
    }
    A->InstanceRegistry.Reserve(WorldA.World.Get());
    B->InstanceRegistry.Reserve(WorldB.World.Get());
    B->InstanceRegistry.Reserve(WorldB.World.Get());
    // 此用例只检查世界账本汇总；实际资源入队/完成/取消发布由ResourcePendingTransitions覆盖。
    A->AdjustPendingInstanceCount(1);
    B->AdjustPendingInstanceCount(2);
    TestEqual(TEXT("初始化只登记两个世界"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().RegisteredWorlds, Before.RegisteredWorlds + 2);
    TestEqual(TEXT("生产更新发布所有世界总数"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().TrackedInstances, Before.TrackedInstances + 3);
    WorldA.Destroy();
    TestEqual(TEXT("销毁一个世界只移除其注册"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().RegisteredWorlds, Before.RegisteredWorlds + 1);
    TestEqual(TEXT("销毁一个世界保留其他实例"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().TrackedInstances, Before.TrackedInstances + 2);
    A->UpdateRuntimeDiagnostics();
    A->Deinitialize();
    TestEqual(TEXT("销毁后迟到更新和重复关闭不覆盖存活世界"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().PendingInstances, Before.PendingInstances + 2);
    WorldB.Destroy();
    const auto After = FGamePlatformVFXDiagnostics::GetRuntimeCounts();
    TestEqual(TEXT("夹具销毁后恢复原有世界数"), After.RegisteredWorlds, Before.RegisteredWorlds);
    TestEqual(TEXT("夹具销毁后恢复原有实例数"), After.TrackedInstances, Before.TrackedInstances);
    TestEqual(TEXT("夹具销毁后恢复原有等待数"), After.PendingInstances, Before.PendingInstances);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformVFXDiagnosticsResourcePendingTest,
    "GamePlatform.VFX.Diagnostics.ResourcePendingTransitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformVFXDiagnosticsResourcePendingTest::RunTest(const FString&)
{
    if (!TestNotNull(TEXT("真实引擎已启动"), GEngine)) return false;
    auto State = MakeShared<FVFXDiagnosticsResourceState>();
    State->Before = FGamePlatformVFXDiagnostics::GetRuntimeCounts();
    State->GameInstance.Reset(NewObject<UGameInstance>(GEngine));
    State->GameInstance->Init();
    if (!TestTrue(TEXT("测试世界取得已初始化VFX服务"), State->Fixture.Initialize(State->GameInstance.Get()))) return false;
    auto* Data = IGamePlatformDataService::Get(*State->GameInstance);
    if (!TestNotNull(TEXT("使用真实Data公开服务"), Data)) return false;
    auto* Service = State->Fixture.Service.Get();

    // 与既有ResourceOwnership测试相同，仅搭建已解析的私有元数据缓存；真实资源仍走公开Data租约。
    State->Definition.Reset(NewObject<UGamePlatformVFXCompositeDefinition>());
    auto* Definition = State->Definition.Get();
    Definition->LogicalId.Namespace = TEXT("presentation.vfx");
    Definition->LogicalId.Name = TEXT("diagnostics_pending");
    Definition->LogicalId.LogicalVersion = 1;
    Definition->DataVersion.SchemaVersion = 1;
    Definition->DataVersion.ContentRevision = 1;
    FGamePlatformVFXCompositeStep Step;
    Step.DefinitionId = TEXT("presentation.vfx.diagnostics_pending_child@1");
    Step.DelaySeconds = 5.0f;
    Definition->Steps.Add(Step);
    Definition->RequiredDefinitions.Add(FPrimaryAssetId(UGamePlatformPrimaryDataAsset::DefinitionAssetType(), Step.DefinitionId));
    // 只改测试自有Transient对象的编辑器属性，不添加生产setter、不修改共享定义/CDO。
    const FArrayProperty* Preloads = FindFProperty<FArrayProperty>(Definition->GetClass(), TEXT("PreloadAssets"));
    if (!TestNotNull(TEXT("夹具可配置本定义的预载资源属性"), Preloads)) return false;
    Preloads->ContainerPtrToValuePtr<TArray<TSoftObjectPtr<UObject>>>(Definition)->Add(
        TSoftObjectPtr<UObject>(FSoftObjectPath(UNiagaraComponent::StaticClass()->GetPathName())));
    if (!TestTrue(TEXT("夹具元数据满足结构和依赖声明校验"), Definition->ValidateDefinition().IsSuccess())) return false;
    const FName DefinitionId = Definition->GetDefinitionId();
    Service->DefinitionCache.FindOrAdd(DefinitionId).Definition = Definition;
    FGamePlatformVFXRequest Request;
    Request.DefinitionId = DefinitionId;
    Request.RequestId = FGuid::NewGuid();
    Request.bAllowFallback = false;
    const auto Playing = Service->Play(Request);
    State->Handle = Playing.Handle;
    if (!TestEqual(TEXT("缓存命中后真实Data资源申请被受理"), Playing.Code, EGamePlatformVFXResultCode::Queued)) return false;
    const auto* Pending = Service->InstanceResources.Find(Playing.Handle.Id);
    if (!TestNotNull(TEXT("资源申请进入生产等待账本"), Pending)) return false;
    State->ResourceLease = Pending->Lease;
    TestEqual(TEXT("资源入队立即发布Pending，不依赖后续Play或Tick"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().PendingInstances, State->Before.PendingInstances + 1);

    Request.RequestId = FGuid::NewGuid();
    const auto Cancelled = Service->Play(Request);
    if (!TestEqual(TEXT("第二次真实资源申请也进入等待"), Cancelled.Code, EGamePlatformVFXResultCode::Queued)) return false;
    const auto* ToCancel = Service->InstanceResources.Find(Cancelled.Handle.Id);
    if (!TestNotNull(TEXT("取消目标持有真实资源租约"), ToCancel)) return false;
    const auto CancelledLease = ToCancel->Lease;
    TestEqual(TEXT("两个资源请求均即时计入等待"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().PendingInstances, State->Before.PendingInstances + 2);
    // 直接检查生产释放边界；随后Stop的其他发布不能掩盖ReleaseInstanceResources自身漏报。
    Service->ReleaseInstanceResources(Cancelled.Handle);
    TestEqual(TEXT("释放等待资源立即发布Pending"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().PendingInstances, State->Before.PendingInstances + 1);
    TestEqual(TEXT("取消实际撤销Data资源租约"), Data->GetLeaseState(CancelledLease), EGamePlatformDataRequestState::Released);
    // 故意投递已撤销租约的旧回执，仅检查拒绝复活，不声称资源或Niagara执行成功。
    Service->HandleSelectedResourcesLoaded(Cancelled.Handle, CancelledLease, FGamePlatformResult::Success());
    TestEqual(TEXT("取消后的迟到回执不改变Pending"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().PendingInstances, State->Before.PendingInstances + 1);
    TestTrue(TEXT("取消后仍可清理实例句柄"), Service->Stop(Cancelled.Handle));

    State->StartedAtSeconds = FPlatformTime::Seconds();
    ADD_LATENT_AUTOMATION_COMMAND(FVFXDiagnosticsResourceWait(State, [this, State]()
    {
        auto* ActualData = IGamePlatformDataService::Get(*State->GameInstance);
        if (TestNotNull(TEXT("完成时真实Data服务仍存活"), ActualData))
            TestEqual(TEXT("UE类资源租约真实完成"), ActualData->GetLeaseState(State->ResourceLease), EGamePlatformDataRequestState::Succeeded);
        TestEqual(TEXT("资源完成后Composite保留活跃实例"), State->Fixture.Service->GetPlaybackSnapshot(State->Handle).State, EGamePlatformVFXPlaybackState::Playing);
        TestEqual(TEXT("完成回执本身立即清除Pending"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().PendingInstances, State->Before.PendingInstances);
        TestEqual(TEXT("完成断言前没有通过实例清理补发计数"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().TrackedInstances, State->Before.TrackedInstances + 1);
        // 本世界不交给引擎Tick；延迟子步骤尚未执行即清理，不借缺失子资产或自然结束刷新Pending。
        State->Fixture.Destroy();
        const auto After = FGamePlatformVFXDiagnostics::GetRuntimeCounts();
        TestEqual(TEXT("资源测试销毁后恢复世界注册数"), After.RegisteredWorlds, State->Before.RegisteredWorlds);
        TestEqual(TEXT("资源测试销毁后恢复实例数"), After.TrackedInstances, State->Before.TrackedInstances);
        TestEqual(TEXT("资源测试销毁后恢复等待数"), After.PendingInstances, State->Before.PendingInstances);
    }));
    return true;
}
#endif

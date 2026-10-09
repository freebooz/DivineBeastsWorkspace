// 本文件属于GamePlatform平台层 GamePlatformSurface，负责回归用例；夹具仅测试作用域，不伪造生产资源成功。
// 中文职责、调用方、参数/单位、失败/取消及资源生命周期见本插件 Docs/AuditRemediation-2026-10-09.md（2026-10-09本轮范围）。
// F10：Transient MPC只验证稳定失配/异步旧代拒绝/释放；不生成资产，也不宣称真实材质链通过。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformSurfaceWorldSubsystem.h"
#include "Settings/GamePlatformSurfaceSettings.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformSurfaceBindingRefreshTest, "GamePlatform.Surface.Binding.StickyFailureAndOldGeneration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformSurfaceBindingRefreshTest::RunTest(const FString&)
{
    auto* Settings = GetMutableDefault<UGamePlatformSurfaceSettings>();
    TGuardValue<bool> RestoreWarn(Settings->bWarnOnMaterialBindingFailure, false);
    TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game, false));
    TStrongObjectPtr<UGamePlatformSurfaceWorldSubsystem> Service(NewObject<UGamePlatformSurfaceWorldSubsystem>(World.Get()));
    TStrongObjectPtr<UMaterialParameterCollection> Collection(NewObject<UMaterialParameterCollection>());
    TStrongObjectPtr<UMaterialParameterCollectionInstance> Instance(NewObject<UMaterialParameterCollectionInstance>());
    Instance->SetCollection(Collection.Get(), World.Get());
    Service->BoundCollection = Collection.Get(); Service->BoundInstance = Instance.Get(); Service->bBindingAttempted = true;
    FGamePlatformSurfaceEnvironmentState State; State.GlobalWetness = 0.5f;
    TestEqual(TEXT("缺八个正式参数明确失配"), Service->ApplyEnvironmentState(State).Status, EGamePlatformSurfaceUpdateStatus::ParameterContractMismatch);
    TestEqual(TEXT("重复相同输入仍失配，不能变Unchanged"), Service->ApplyEnvironmentState(State).Status, EGamePlatformSurfaceUpdateStatus::ParameterContractMismatch);
    FGamePlatformDataLease Lease; Lease.LeaseId = FGuid::NewGuid(); Lease.ScopeId = FGuid::NewGuid(); Lease.Generation = 4;
    Lease.IssuerProof = FGuid::NewGuid(); Lease.ResourcePaths.Add(FSoftObjectPath(TEXT("/Engine/Transient.SurfaceFixture")));
    Service->BindingLease = Lease; Service->BindingGeneration = 9;
    Service->HandleMaterialBindingLoaded(8, Lease, FGamePlatformResult::Success());
    TestEqual(TEXT("旧代成功不覆盖当前绑定"), Service->BoundCollection.Get(), Collection.Get());
    Service->ReleaseMaterialBinding();
    Service->HandleMaterialBindingLoaded(9, Lease, FGamePlatformResult::Success());
    TestNull(TEXT("取消后的旧完成不得复活MPC"), Service->BoundCollection.Get());
    Service->bBindingAttempted = true;
    Service->LastBindingResult.Status = EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable;
    const int64 Before = Service->BindingGeneration;
    TestEqual(TEXT("无资源重复更新保持错误"), Service->ApplyEnvironmentState(State).Status, EGamePlatformSurfaceUpdateStatus::MaterialBindingUnavailable);
    TestEqual(TEXT("普通状态事件不重复申请Data"), Service->BindingGeneration, Before);
    Service->bClosing = true;
    TestEqual(TEXT("世界退出拒绝新状态"), Service->ApplyEnvironmentState(State).Status, EGamePlatformSurfaceUpdateStatus::InvalidWorld);
    World->DestroyWorld(false);
    return true;
}
#endif

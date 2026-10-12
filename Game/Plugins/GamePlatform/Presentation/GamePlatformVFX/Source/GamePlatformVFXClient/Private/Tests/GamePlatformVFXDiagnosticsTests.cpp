// 跨世界诊断回归：验证同进程多世界计数隔离及生产子系统的初始化/注销接线；不证明Niagara视觉质量。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Diagnostics/GamePlatformVFXDiagnostics.h"
#include "Diagnostics/GamePlatformVFXRuntimeCountRegistry.h"
#include "Subsystems/GamePlatformVFXWorldSubsystem.h"
#include "Subsystems/SubsystemCollection.h"
#include "Settings/GamePlatformVFXSettings.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

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
    // 夹具仅关闭旧Catalog预载入口，避免用外部资源成败解释诊断账本；仍调用生产Initialize/Deinitialize。
    TGuardValue<TArray<TSoftObjectPtr<UGamePlatformVFXCatalog>>> EmptyCatalogs(
        GetMutableDefault<UGamePlatformVFXSettings>()->StartupCatalogs, {});
    const auto Before = FGamePlatformVFXDiagnostics::GetRuntimeCounts();
    const TStrongObjectPtr<UWorld> WorldA(UWorld::CreateWorld(EWorldType::Game, false));
    const TStrongObjectPtr<UWorld> WorldB(UWorld::CreateWorld(EWorldType::Game, false));
    const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> A(NewObject<UGamePlatformVFXWorldSubsystem>(WorldA.Get()));
    const TStrongObjectPtr<UGamePlatformVFXWorldSubsystem> B(NewObject<UGamePlatformVFXWorldSubsystem>(WorldB.Get()));
    FSubsystemCollection<UGamePlatformVFXWorldSubsystem> CollectionA;
    FSubsystemCollection<UGamePlatformVFXWorldSubsystem> CollectionB;
    A->Initialize(CollectionA);
    B->Initialize(CollectionB);
    A->InstanceRegistry.Reserve(WorldA.Get());
    B->InstanceRegistry.Reserve(WorldB.Get());
    B->InstanceRegistry.Reserve(WorldB.Get());
    A->PendingInstanceCount = 1;
    B->PendingInstanceCount = 2;
    A->UpdateRuntimeDiagnostics();
    B->UpdateRuntimeDiagnostics();
    TestEqual(TEXT("生产初始化注册两个独立世界"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().RegisteredWorlds, Before.RegisteredWorlds + 2);
    TestEqual(TEXT("生产更新发布所有世界总数"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().TrackedInstances, Before.TrackedInstances + 3);
    A->Deinitialize();
    TestEqual(TEXT("生产关闭只移除当前世界"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().TrackedInstances, Before.TrackedInstances + 2);
    A->UpdateRuntimeDiagnostics();
    A->Deinitialize();
    TestEqual(TEXT("关闭后更新和重复关闭不覆盖存活世界"), FGamePlatformVFXDiagnostics::GetRuntimeCounts().PendingInstances, Before.PendingInstances + 2);
    B->Deinitialize();
    const auto After = FGamePlatformVFXDiagnostics::GetRuntimeCounts();
    TestEqual(TEXT("夹具关闭后恢复原有世界数"), After.RegisteredWorlds, Before.RegisteredWorlds);
    TestEqual(TEXT("夹具关闭后恢复原有实例数"), After.TrackedInstances, Before.TrackedInstances);
    WorldA->DestroyWorld(false);
    WorldB->DestroyWorld(false);
    return true;
}
#endif

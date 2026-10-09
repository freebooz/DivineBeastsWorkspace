// 项目竞技服务器组合路由回归：两个真实World/GameMode独立保留状态，结束其中一个不覆盖另一世界。
#include "Server/DivineBeastsArenaServerProjectExtension.h"
#include "Framework/GamePlatformArenaGameMode.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsArenaWorldOwnershipTest,
    "DivineBeasts.Arena.Server.WorldOwnership", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsArenaWorldOwnershipTest::RunTest(const FString& Parameters)
{
    TStrongObjectPtr<UWorld> A(UWorld::CreateWorld(EWorldType::Game, false));
    TStrongObjectPtr<UWorld> B(UWorld::CreateWorld(EWorldType::Game, false));
    FDivineBeastsArenaServerProjectExtension Extension;
    auto* ModeA = A->SpawnActor<AGamePlatformArenaGameMode>();
    auto* ModeB = B->SpawnActor<AGamePlatformArenaGameMode>();
    TestNotNull(TEXT("真实世界A有独立GameMode"), ModeA);
    TestNotNull(TEXT("真实世界B有独立GameMode"), ModeB);
    if (ModeA && ModeB)
    {
        Extension.WorldAssemblies.Add(ModeA);
        Extension.WorldAssemblies.Add(ModeB);
        Extension.ReleaseWorldAssembly(ModeB);
        Extension.ReleaseWorldAssembly(ModeB);
        TestTrue(TEXT("重复结束B保留A所有权"), Extension.WorldAssemblies.Contains(ModeA));
        TestFalse(TEXT("B自身桶撤销"), Extension.WorldAssemblies.Contains(ModeB));
        // 真实WorldCleanup事件应回收A的桶；这里不伪造资源成功，也不宣称比赛出生通过。
        A->DestroyWorld(false);
        TestEqual(TEXT("世界清理及时回收桶"), Extension.WorldAssemblies.Num(), 0);
    }
    if (!ModeA || !ModeB) A->DestroyWorld(false);
    B->DestroyWorld(false);
    return true;
}
#endif

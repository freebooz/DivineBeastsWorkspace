#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "UI/DivineBeastsArenaUIScreenCatalog.h"
#include "UI/HUD/DivineBeastsArenaHUD.h"
#include "UI/GamePlatformMobaArenaHUDBase.h"
#include "UI/GamePlatformMobaArenaModalScreenBase.h"
#include "UI/GamePlatformMobaArenaScreenBase.h"
#include "UI/Screens/DivineBeastsArenaHeroSelectionScreen.h"
#include "UI/Screens/DivineBeastsArenaScreenBase.h"
#include "UI/Screens/DivineBeastsMatchFoundReadyScreen.h"
#include "UI/Screens/DivineBeastsMatchmakingScreen.h"
#include "UI/Screens/DivineBeastsPostMatchResultScreen.h"
#include "UI/Screens/DivineBeastsScoreboardScreen.h"

/**
 * 验证竞技UI表面只归DBAArena所有，并保持项目层 → MOBA通用层 → 平台层的单向继承。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsArenaUISurfaceInventoryTest,
    "DivineBeasts.Arena.UI.SurfaceInventory",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsArenaUISurfaceInventoryTest::RunTest(const FString&)
{
    const TArray<FDivineBeastsArenaUISurfaceDescriptor>& Surfaces =
        FDivineBeastsArenaUIScreenCatalog::GetSurfaces();

    TestEqual(TEXT("DBAArena应拥有5个Screen加1个ArenaHUD"), Surfaces.Num(), 6);

    TSet<FName> UniqueIds;
    int32 ScreenCount = 0;
    int32 HUDCount = 0;
    for (const FDivineBeastsArenaUISurfaceDescriptor& Surface : Surfaces)
    {
        TestTrue(TEXT("竞技SurfaceId必须有效"), !Surface.SurfaceId.IsNone());
        TestFalse(TEXT("竞技SurfaceId必须唯一"), UniqueIds.Contains(Surface.SurfaceId));
        UniqueIds.Add(Surface.SurfaceId);

        TestTrue(
            TEXT("竞技软资源路径必须归DBAArena挂载点"),
            Surface.WidgetClassPath.StartsWith(TEXT("/DBAArena/")));

        if (Surface.Kind == EDivineBeastsArenaUISurfaceKind::Screen)
        {
            ++ScreenCount;
        }
        else if (Surface.Kind == EDivineBeastsArenaUISurfaceKind::HUD)
        {
            ++HUDCount;
        }
    }

    TestEqual(TEXT("竞技Screen数量"), ScreenCount, 5);
    TestEqual(TEXT("竞技HUD数量"), HUDCount, 1);

    for (const FName RequiredId : {
         FName(TEXT("UI.Screen.Matchmaking")),
         FName(TEXT("UI.Screen.MatchFoundReady")),
         FName(TEXT("UI.Screen.ArenaHeroSelection")),
         FName(TEXT("UI.Screen.Scoreboard")),
         FName(TEXT("UI.Screen.PostMatchResult")),
         FName(TEXT("UI.HUD.Arena"))})
    {
        TestNotNull(
            *FString::Printf(
                TEXT("DBAArena必须拥有表面 %s"),
                *RequiredId.ToString()),
            FDivineBeastsArenaUIScreenCatalog::Find(RequiredId));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsArenaUIHierarchyTest,
    "DivineBeasts.Arena.UI.Hierarchy",
    EAutomationTestFlags_ApplicationContextMask |
    EAutomationTestFlags::EngineFilter)

bool FDivineBeastsArenaUIHierarchyTest::RunTest(const FString&)
{
    TestTrue(
        TEXT("项目ArenaHUD必须继承MOBA通用ArenaHUD"),
        UDivineBeastsArenaHUD::StaticClass()->IsChildOf(
            UGamePlatformMobaArenaHUDBase::StaticClass()));

    TestTrue(
        TEXT("项目Arena普通页面基类必须继承MOBA通用ArenaScreen"),
        UDivineBeastsArenaScreenBase::StaticClass()->IsChildOf(
            UGamePlatformMobaArenaScreenBase::StaticClass()));

    TestTrue(
        TEXT("匹配成功准备页必须使用MOBA竞技Modal基类"),
        UDivineBeastsMatchFoundReadyScreen::StaticClass()->IsChildOf(
            UGamePlatformMobaArenaModalScreenBase::StaticClass()));

    TestTrue(
        TEXT("四个普通竞技页面必须继承项目ArenaScreen基类"),
        UDivineBeastsMatchmakingScreen::StaticClass()->IsChildOf(
            UDivineBeastsArenaScreenBase::StaticClass()) &&
        UDivineBeastsArenaHeroSelectionScreen::StaticClass()->IsChildOf(
            UDivineBeastsArenaScreenBase::StaticClass()) &&
        UDivineBeastsScoreboardScreen::StaticClass()->IsChildOf(
            UDivineBeastsArenaScreenBase::StaticClass()) &&
        UDivineBeastsPostMatchResultScreen::StaticClass()->IsChildOf(
            UDivineBeastsArenaScreenBase::StaticClass()));

    return true;
}

#endif

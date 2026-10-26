#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Layers/DivineBeastsWorldHUDRoute.h"
#include "Identity/DivineBeastsProjectCatalog.h"

/** 仅验证世界HUD路由，不创建Widget/世界/角色或修改已保存资产。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsWorldHUDRouteTest,
    "DivineBeasts.UI.Architecture.WorldHUDRouting",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsWorldHUDRouteTest::RunTest(const FString&)
{
    const FName Open=FDivineBeastsProjectCatalog::GetOpenWorldServerRole();
    const FName Village=FDivineBeastsProjectCatalog::GetVillageServerRole();
    const FName Arena=FDivineBeastsProjectCatalog::GetMainArenaServerRole();
    TestTrue(TEXT("常驻世界使用通用世界HUD"),FDivineBeastsWorldHUDRoute::IsSupportedWorldRole(Open));
    TestTrue(TEXT("新手村使用通用世界HUD"),FDivineBeastsWorldHUDRoute::IsSupportedWorldRole(Village));
    TestFalse(TEXT("竞技场不借用普通世界HUD"),FDivineBeastsWorldHUDRoute::IsSupportedWorldRole(Arena));
    TestEqual(TEXT("常驻世界索引0"),FDivineBeastsWorldHUDRoute::ResolveSwitcherIndex(Open,NAME_None),0);
    TestEqual(TEXT("新手村索引1"),FDivineBeastsWorldHUDRoute::ResolveSwitcherIndex(Village,NAME_None),1);
    TestEqual(TEXT("教学索引2"),
        FDivineBeastsWorldHUDRoute::ResolveSwitcherIndex(
            Village,FDivineBeastsProjectCatalog::GetVillageTutorialExperience()),2);
    TestEqual(TEXT("训练索引3"),
        FDivineBeastsWorldHUDRoute::ResolveSwitcherIndex(
            Village,FDivineBeastsProjectCatalog::GetVillageTrainingExperience()),3);
    TestEqual(TEXT("未知世界角色不可选择HUD"),
        FDivineBeastsWorldHUDRoute::ResolveSwitcherIndex(TEXT("Unknown.World"),NAME_None),INDEX_NONE);
    TestEqual(TEXT("体验覆盖基础角色索引的原有优先级保持"),
        FDivineBeastsWorldHUDRoute::ResolveSwitcherIndex(
            Open,FDivineBeastsProjectCatalog::GetVillageTutorialExperience()),2);
    return true;
}

#endif

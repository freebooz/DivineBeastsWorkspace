// SSLM寒冰箭视觉路由自动化：真实C++过滤器的正例与拒绝例，不替代Niagara GPU或联机验收。
// 此测试仅在Editor运行，不生成资产、不会修改权威伤害和玩家资料。
#include "CoreMinimal.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Development/DivineBeastsPlaceholderMagicVisualPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsIceArrowVisualPolicyTest,
    "DivineBeasts.Presentation.IceArrow.PlaceholderVisualPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsIceArrowVisualPolicyTest::RunTest(const FString&)
{
    using namespace DivineBeastsPlaceholderMagicVisualPolicy;
    // 正式地图与对应PIE世界通过；其它世界（包括冒用相同关卡短名）严格拒绝。
    TestTrue(TEXT("正式Village允许启动本地占位表现"), IsVillagePackageName(TEXT("/DBAWorldPack_Village/Maps/L_Village_Start")));
    TestTrue(TEXT("PIE副本允许启动本地占位表现"), IsVillagePackageName(TEXT("/DBAWorldPack_Village/Maps/UEDPIE_0_L_Village_Start")));
    TestFalse(TEXT("Arena不得启用Village占位表现"), IsVillagePackageName(TEXT("/DBAArena/Maps/L_Village_Start")));
    TestFalse(TEXT("伪装名字与同目录其它地图不得放行"), IsVillagePackageName(TEXT("/DBAWorldPack_Village/Maps/L_Village_Start_Test")));
    TestFalse(TEXT("近似PIE前缀不得放行"), IsVillagePackageName(TEXT("/DBAWorldPack_Village/Maps/UEDPIE_L_Village_Start")));

    TestTrue(TEXT("子鼠寒冰箭占位技能属于许可列表"),
        IsApprovedPlaceholderSpell(TEXT("dba.ability.rat_placeholder_frostbolt@1"), TEXT("Frostbolt")));
    TestTrue(TEXT("亥猪寒冰箭占位技能属于许可列表"),
        IsApprovedPlaceholderSpell(TEXT("dba.ability.boar_placeholder_frostbolt@1"), TEXT("Frostbolt")));
    TestFalse(TEXT("非十二生肖身份不得伪装占位技能"),
        IsApprovedPlaceholderSpell(TEXT("dba.ability.unknown_placeholder_frostbolt@1"), TEXT("Frostbolt")));
    TestFalse(TEXT("其它法术样式不能借占位技能身份获取未登记VFX"),
        IsApprovedPlaceholderSpell(TEXT("dba.ability.rat_placeholder_frostbolt@1"), TEXT("ArcaneMissiles")));
    TestFalse(TEXT("非法外观样式不得获准"),
        IsApprovedPlaceholderSpell(TEXT("dba.ability.rat_placeholder_frostbolt@1"), TEXT("OtherSpell")));
    TestFalse(TEXT("旧版本身份不得获准"),
        IsApprovedPlaceholderSpell(TEXT("dba.ability.rat_placeholder_frostbolt@2"), TEXT("Frostbolt")));
    return true;
}
#endif

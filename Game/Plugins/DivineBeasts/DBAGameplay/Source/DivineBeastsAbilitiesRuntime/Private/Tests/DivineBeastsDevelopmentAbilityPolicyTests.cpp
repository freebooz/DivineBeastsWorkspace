// 项目层开发授权策略回归：纯值输入覆盖正常、未启用、发行构建和跨英雄映射；不伪造真实资产授予。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Development/DivineBeastsDevelopmentAbilityPolicy.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDevelopmentAbilityPolicyTest,
    "DivineBeasts.Abilities.DevelopmentOverlayPolicy",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsDevelopmentAbilityPolicyTest::RunTest(const FString&)
{
    using namespace DivineBeasts::DevelopmentAbilities;
    const FName Formal(TEXT("dba.abilityset.rat_formal@1"));
    for (int32 Mask = 0; Mask < 8; ++Mask)
    {
        TestEqual(TEXT("编译资格、配置与命令行必须同时满足"),
            IsEnabled((Mask & 1) != 0, (Mask & 2) != 0, (Mask & 4) != 0), Mask == 7);
    }
    TestEqual(TEXT("关闭开发覆盖保留正式授权"),
        ResolveSetId(TEXT("Hero.Zodiac.Rat"), Formal, false, TEXT("dba.abilityset.rat_dev@1")), Formal);
    TestEqual(TEXT("跨英雄映射不得替换正式授权"),
        ResolveSetId(TEXT("Hero.Zodiac.Rat"), Formal, true, TEXT("dba.abilityset.ox_dev@1")), Formal);
    TestEqual(TEXT("非法英雄和任意集合不得提升权限"),
        ResolveSetId(TEXT("Hero.Zodiac.Invalid"), NAME_None, true, TEXT("dba.abilityset.invalid_dev@1")), NAME_None);
    TestEqual(TEXT("空映射不虚构技能集合"),
        ResolveSetId(TEXT("Hero.Zodiac.Rat"), NAME_None, true, TEXT("")), NAME_None);
    for (FName HeroId : FDivineBeastsHeroCatalog::GetCoreHeroIds())
    {
        const FString Suffix = HeroId.ToString().RightChop(12).ToLower();
        const FString DevelopmentId = FString(TEXT("dba.abilityset.")) + Suffix + TEXT("_dev@1");
        TestEqual(TEXT("十二生肖各自开发映射可替代空正式集合"),
            ResolveSetId(HeroId, NAME_None, true, DevelopmentId), FName(*DevelopmentId));
    }
    return true;
}
#endif

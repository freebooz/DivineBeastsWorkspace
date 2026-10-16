// SSLM项目层自动化测试：核对MainArena的真实配置是否从项目配置层读取。
// 仅在正式SSLM主工程运行；它不测试Niagara原生System、粒子或GPU帧耗时。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Settings/GamePlatformVFXSettings.h"
#include "Settings/GamePlatformVFXWorldBudgetRule.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsMainArenaVFXConfigTest,
    "DivineBeasts.VFX.MainArena.LoadedBudgetSettings",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsMainArenaVFXConfigTest::RunTest(const FString&)
{
    // 通用DevHost不是SSLM主工程，不应强制消费SSLM竞技场项目覆盖设置。
    if (FString(FApp::GetProjectName()) != TEXT("DivineBeastsArena"))
    {
        AddWarning(TEXT("本项只验正式DivineBeastsArena主工程，当前DevHost不参与MainArena预算验收。"));
        return true;
    }

    const UGamePlatformVFXSettings* Settings = GetDefault<UGamePlatformVFXSettings>();
    if (!TestNotNull(TEXT("GamePlatformVFX独立配置类别必须正确加载"), Settings)) return false;

    TestTrue(TEXT("SSLM项目层显式启用竞技场专用请求预算"), Settings->bUseWorldBudgetRules);
    TestEqual(TEXT("MainArena恰有四档独立预算"), Settings->WorldBudgetRules.Num(), 4);

    struct FCandidate { EGamePlatformVFXQualityTier Tier; int32 Combat; int32 Status; int32 Ambient; };
    const FCandidate Expected[] = {
        {EGamePlatformVFXQualityTier::Low, 40, 24, 8},
        {EGamePlatformVFXQualityTier::Medium, 58, 34, 8},
        {EGamePlatformVFXQualityTier::High, 80, 50, 10},
        {EGamePlatformVFXQualityTier::Epic, 120, 72, 12}
    };
    for (const FCandidate& Candidate : Expected)
    {
        const FGamePlatformVFXWorldBudgetRule* Rule = Settings->WorldBudgetRules.FindByPredicate(
            [&Candidate](const FGamePlatformVFXWorldBudgetRule& Item)
            {
                return Item.WorldIdPrefix == TEXT("Experience.MainArena.Main") && Item.QualityTier == Candidate.Tier;
            });
        if (!TestNotNull(TEXT("当前画质的竞技场体验命名空间必须可匹配"), Rule)) continue;
        TestEqual(TEXT("对应画质的普通请求软上限"), Rule->MaxCombatRequests, Candidate.Combat);
        TestEqual(TEXT("对应画质的状态请求软上限"), Rule->MaxStatusRequests, Candidate.Status);
        TestEqual(TEXT("对应画质的环境请求软上限"), Rule->MaxAmbientRequests, Candidate.Ambient);
    }

    // 配置加载成功不等于System/EffectType真实限量；原生预算和GPU仍须单独验收。
    return true;
}
#endif

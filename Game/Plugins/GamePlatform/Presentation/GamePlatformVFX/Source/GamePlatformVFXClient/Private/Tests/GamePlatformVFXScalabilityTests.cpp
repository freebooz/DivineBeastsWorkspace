#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Definitions/GamePlatformVFXInstantDefinition.h"
#include "Pooling/GamePlatformVFXPoolingPolicy.h"
#include "Scalability/GamePlatformVFXScalabilityPolicy.h"
#include "Settings/GamePlatformVFXSettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXHardBudgetTest,
    "GamePlatform.VFX.Scalability.HardBudget",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXHardBudgetTest::RunTest(const FString& Parameters)
{
    UGamePlatformVFXSettings* Settings = NewObject<UGamePlatformVFXSettings>();
    Settings->MaxActiveInstances = 8;
    Settings->MaxStatusInstances = 6;
    Settings->MaxAmbientInstances = 3;
    Settings->HardMaxTrackedInstances = 10;

    FGamePlatformVFXRequest Ambient;
    Ambient.Importance = EGamePlatformVFXImportance::Ambient;
    TestTrue(
        TEXT("Ambient在自身软预算内允许"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Ambient, 2, *Settings));
    TestFalse(
        TEXT("Ambient达到低优先级软预算后拒绝，为战斗预留容量"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Ambient, 3, *Settings));

    FGamePlatformVFXRequest Status;
    Status.Importance = EGamePlatformVFXImportance::Status;
    TestTrue(
        TEXT("Status可以使用Ambient保留区之后的容量"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Status, 3, *Settings));
    TestFalse(
        TEXT("Status达到自身累计软预算后拒绝"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Status, 6, *Settings));

    FGamePlatformVFXRequest Combat;
    Combat.Importance = EGamePlatformVFXImportance::Combat;
    TestTrue(
        TEXT("Combat可以使用Status预算之后的保留容量"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Combat, 6, *Settings));
    TestFalse(
        TEXT("Combat达到普通总软预算后拒绝"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Combat, 8, *Settings));

    FGamePlatformVFXRequest Critical;
    Critical.Importance = EGamePlatformVFXImportance::Critical;
    TestTrue(
        TEXT("Critical允许突破普通软预算"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Critical, 8, *Settings));
    TestFalse(
        TEXT("Critical也不得突破绝对硬上限"),
        FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Critical, 10, *Settings));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXCriticalPoolingTest,
    "GamePlatform.VFX.Scalability.CriticalPooling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXCriticalPoolingTest::RunTest(const FString& Parameters)
{
    UGamePlatformVFXSettings* Settings = NewObject<UGamePlatformVFXSettings>();
    Settings->bEnablePooling = true;

    const UGamePlatformVFXInstantDefinition* Definition =
        NewObject<UGamePlatformVFXInstantDefinition>();

    FGamePlatformVFXRequest Critical;
    Critical.Importance = EGamePlatformVFXImportance::Critical;

    TestTrue(
        TEXT("Critical重要度不能强制关闭Niagara原生池"),
        FGamePlatformVFXPoolingPolicy::ShouldUseNiagaraPool(
            *Definition,
            Critical,
            *Settings));
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformVFXWorldBudgetRuleTest,
    "GamePlatform.VFX.Scalability.WorldBudgetRule",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformVFXWorldBudgetRuleTest::RunTest(const FString& Parameters)
{
    // 测试中立命名空间、品质档和硬限制，项目运行时不会硬编码测试世界名称。
    UGamePlatformVFXSettings* Settings = NewObject<UGamePlatformVFXSettings>();
    Settings->MaxActiveInstances = 50;
    Settings->MaxStatusInstances = 30;
    Settings->MaxAmbientInstances = 10;
    Settings->HardMaxTrackedInstances = 60;
    Settings->bUseWorldBudgetRules = true;
    FGamePlatformVFXWorldBudgetRule Rule;
    Rule.WorldIdPrefix = TEXT("World.TestBattle");
    Rule.QualityTier = EGamePlatformVFXQualityTier::High;
    Rule.MaxCombatRequests = 8;
    Rule.MaxStatusRequests = 5;
    Rule.MaxAmbientRequests = 2;
    Settings->WorldBudgetRules.Add(Rule);

    FGamePlatformVFXRequest Request;
    Request.WorldId = TEXT("World.TestBattle.01");
    Request.QualityTier = EGamePlatformVFXQualityTier::High;
    Request.Importance = EGamePlatformVFXImportance::Ambient;
    TestTrue(TEXT("环境低于专有配额可播"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 1, *Settings));
    TestFalse(TEXT("环境达到专有配额拒绝"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 2, *Settings));
    Request.Importance = EGamePlatformVFXImportance::Status;
    TestFalse(TEXT("状态达到专有配额拒绝"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 5, *Settings));
    Request.Importance = EGamePlatformVFXImportance::Combat;
    TestTrue(TEXT("战斗低于专有配额可播"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 7, *Settings));
    TestFalse(TEXT("战斗达到专有配额拒绝"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 8, *Settings));
    Request.Importance = EGamePlatformVFXImportance::Critical;
    TestTrue(TEXT("关键效果仍只受硬限"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 59, *Settings));
    TestFalse(TEXT("关键效果不得突破硬限"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 60, *Settings));

    Request.Importance = EGamePlatformVFXImportance::Combat;
    Request.WorldId = TEXT("World.TestBattleOther");
    TestTrue(TEXT("拒绝不合法字头匹配"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 8, *Settings));
    Request.WorldId = TEXT("World.TestBattle.01");
    Request.QualityTier = EGamePlatformVFXQualityTier::Low;
    TestTrue(TEXT("画质不同不套用高画质规则"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 8, *Settings));
    Request.QualityTier = EGamePlatformVFXQualityTier::High;
    // 体验与世界属于不同命名空间，MainArena项目配置不能因WorldId是World.MainArena而失效。
    FGamePlatformVFXWorldBudgetRule ArenaRule;
    ArenaRule.WorldIdPrefix = TEXT("Experience.MainArena.Main");
    ArenaRule.QualityTier = EGamePlatformVFXQualityTier::High;
    ArenaRule.MaxCombatRequests = 7;
    ArenaRule.MaxStatusRequests = 4;
    ArenaRule.MaxAmbientRequests = 2;
    Settings->WorldBudgetRules.Add(ArenaRule);
    Request.WorldId = TEXT("World.MainArena.001");
    Request.ExperienceId = TEXT("Experience.MainArena.Main");
    TestFalse(TEXT("真实体验匹配专用限制"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 7, *Settings));
    Request.ExperienceId = TEXT("Experience.MainArena.MainOther");
    TestTrue(TEXT("不能匹配相邻体验名称"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 7, *Settings));
    Request.ExperienceId = NAME_None;
    Request.WorldId = TEXT("Experience.MainArena.Main");
    TestTrue(TEXT("Experience域不能错误使用WorldId匹配"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 7, *Settings));
    Request.WorldId = TEXT("World.TestBattle.01");
    Request.ExperienceId = TEXT("Experience.MainArena.Main");
    TestFalse(TEXT("体验预算应优先按最长前缀命中"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 7, *Settings));
    Request.ExperienceId = NAME_None;
    TestTrue(TEXT("不携带ExperienceId保持旧World配置效果"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 7, *Settings));

    Settings->bUseWorldBudgetRules = false;
    TestTrue(TEXT("关闭专用规则保持旧行为"), FGamePlatformVFXScalabilityPolicy::ShouldSpawn(Request, 8, *Settings));
    return true;
}
#endif

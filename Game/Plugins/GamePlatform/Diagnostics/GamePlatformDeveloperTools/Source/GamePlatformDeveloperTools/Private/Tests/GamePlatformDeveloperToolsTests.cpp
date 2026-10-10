// 平台Editor工具回归：只创建瞬态对象和调用真实规则，不保存资产、不修改全局配置或运行时业务状态。
#include "Misc/AutomationTest.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/Blueprint.h"
#include "Animation/AnimInstance.h"
#include "GameFramework/Actor.h"
#include "Settings/GamePlatformValidationSettings.h"
#include "UObject/Package.h"

#include "Performance/GamePlatformPerformanceTestRunner.h"
#include "Review/GamePlatformReviewTypes.h"
#include "Validation/GamePlatformValidationService.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsRulesTest,
    "GamePlatform.DeveloperTools.Rules",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsRulesTest::RunTest(const FString& Parameters)
{
    const TArray<FGamePlatformValidationRule>& Rules = FGamePlatformValidationService::GetRules();
    TestTrue(TEXT("Built-in rules are registered"), Rules.Num() >= 10);
    TestNotNull(TEXT("Dependency rule exists"), FGamePlatformValidationService::FindRule(TEXT("GP.Dependency")));
    TestNotNull(TEXT("Inheritance boundary rule exists"), FGamePlatformValidationService::FindRule(TEXT("GP.InheritanceBoundary")));
    TestTrue(
        TEXT("Removed Element tag is rejected"),
        FGamePlatformValidationService::IsRemovedLegacyGameplayTag(TEXT("Element.Fire")));
    TestFalse(
        TEXT("Current neutral tag is not rejected"),
        FGamePlatformValidationService::IsRemovedLegacyGameplayTag(TEXT("Ability.Active")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsAllowlistTest,
    "GamePlatform.DeveloperTools.Allowlist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsAllowlistTest::RunTest(const FString& Parameters)
{
    FGamePlatformValidationAllowlistEntry Entry;
    Entry.RuleId = TEXT("GP.Naming");
    Entry.Target = TEXT("/Game/Test");
    Entry.Reason = TEXT("Temporary migration");
    Entry.Owner = TEXT("Owner");
    Entry.ApprovedBy = TEXT("Reviewer");
    Entry.CreatedAt = FDateTime::UtcNow();
    Entry.ExpiresAt = Entry.CreatedAt + FTimespan::FromDays(1);
    Entry.Ticket = TEXT("DEV-1");

    TArray<FGamePlatformValidationAllowlistEntry> Entries{Entry};

    TestTrue(
        TEXT("Active allowlist applies"),
        FGamePlatformValidationService::IsAllowlisted(
            Entry.RuleId,
            Entry.Target,
            Entries,
            Entry.CreatedAt));

    TestFalse(
        TEXT("Expired allowlist no longer applies"),
        FGamePlatformValidationService::IsAllowlisted(
            Entry.RuleId,
            Entry.Target,
            Entries,
            Entry.ExpiresAt + FTimespan::FromSeconds(1)));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsReportPathTest,
    "GamePlatform.DeveloperTools.ReportPath",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsReportPathTest::RunTest(const FString& Parameters)
{
    FString Directory;
    FString Error;
    TestFalse(
        TEXT("Path traversal RunId is rejected"),
        FGamePlatformValidationService::ResolveSafeReportDirectory(
            TEXT("../escape"),
            Directory,
            Error));

    TestTrue(
        TEXT("Secret masking never returns original full value"),
        FGamePlatformValidationService::MaskSensitiveValue(TEXT("abcdef"))
            != TEXT("abcdef"));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsReviewTest,
    "GamePlatform.DeveloperTools.ManualReview",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsReviewTest::RunTest(const FString& Parameters)
{
    UGamePlatformReviewCase* ReviewCase = NewObject<UGamePlatformReviewCase>();
    ReviewCase->CaseId = TEXT("Review.Case");
    ReviewCase->Category = TEXT("Smoke");
    ReviewCase->Description = TEXT("Manual smoke review");
    ReviewCase->Steps = { TEXT("Observe expected UI") };
    ReviewCase->ExpectedResult = TEXT("Expected UI is correct");
    ReviewCase->RequiredEvidence = { TEXT("Screenshot") };

    FString Error;
    TestTrue(TEXT("Review case is complete"), ReviewCase->IsCaseComplete(Error));

    UGamePlatformReviewSubsystem* Subsystem = NewObject<UGamePlatformReviewSubsystem>();
    UGamePlatformReviewReport* Report =
        Subsystem->CreatePendingReport(ReviewCase, TEXT("Run-1"), TEXT("Build-1"), TEXT("Content-1"));

    TestNotNull(TEXT("Pending report created"), Report);
    TestEqual(
        TEXT("AI-created report remains NotRun"),
        Report->Status,
        EGamePlatformReviewStatus::NotRun);

    TArray<FString> Evidence{TEXT("evidence.png")};
    TestFalse(
        TEXT("Without explicit human confirmation report cannot be marked Passed"),
        Subsystem->ApplyHumanDecision(
            Report,
            EGamePlatformReviewStatus::Passed,
            TEXT("Reviewer"),
            TEXT(""),
            Evidence,
            false,
            Error));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformDeveloperToolsPerformanceTest,
    "GamePlatform.DeveloperTools.PerformanceBaseline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformDeveloperToolsPerformanceTest::RunTest(const FString& Parameters)
{
    UGamePlatformPerformanceTestRunner* Runner = NewObject<UGamePlatformPerformanceTestRunner>();

    FGamePlatformPerformanceBaseline A;
    A.ScenarioId = TEXT("Scenario.MainArena");
    A.HardwareProfile = TEXT("HW-A");
    A.EngineVersion = TEXT("5.8");
    A.Samples = 100;

    FGamePlatformPerformanceBaseline B = A;

    FString Reason;
    TestTrue(TEXT("Compatible baseline can compare"), Runner->CanCompareBaselines(A, B, Reason));

    B.HardwareProfile = TEXT("HW-B");
    TestFalse(TEXT("Different hardware cannot compare"), Runner->CanCompareBaselines(A, B, Reason));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformNamingBlueprintParentPrefixesTest,
    "GamePlatform.DeveloperTools.Naming.BlueprintParentPrefixes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

/** 验证蓝图父类事实而非资产名决定领域前缀，覆盖原生中间派生、配置覆盖和专用蓝图优先级。 */
bool FGamePlatformNamingBlueprintParentPrefixesTest::RunTest(const FString&)
{
    UGamePlatformValidationSettings* Settings = NewObject<UGamePlatformValidationSettings>();
    UBlueprint* Blueprint = NewObject<UBlueprint>();
    // 只通过引擎反射路径加载类型：测试无需给平台工具增加GAS头文件、链接或项目类型依赖。
    UClass* AbilityClass = LoadObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.GameplayAbility"));
    UClass* JumpAbilityClass = LoadObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.GameplayAbility_CharacterJump"));
    UClass* EffectClass = LoadObject<UClass>(nullptr, TEXT("/Script/GameplayAbilities.GameplayEffect"));
    UClass* WidgetBlueprintClass = LoadObject<UClass>(nullptr, TEXT("/Script/UMGEditor.WidgetBlueprint"));
    if (!TestNotNull(TEXT("启用GAS宿主的技能基类"), AbilityClass)
        || !TestNotNull(TEXT("引擎原生跳跃技能派生类"), JumpAbilityClass)
        || !TestNotNull(TEXT("启用GAS宿主的效果基类"), EffectClass)
        || !TestNotNull(TEXT("Editor宿主的WidgetBlueprint类型"), WidgetBlueprintClass))
    {
        return false;
    }
    Blueprint->ParentClass = AbilityClass;
    TestEqual(TEXT("技能蓝图采用领域GA前缀"), Settings->ResolveAssetPrefix(Blueprint), FString(TEXT("GA_")));
    Blueprint->ParentClass = JumpAbilityClass;
    TestEqual(TEXT("中间原生派生仍沿祖先识别GameplayAbility"), Settings->ResolveAssetPrefix(Blueprint), FString(TEXT("GA_")));
    Settings->AssetClassPrefixes.Add(TEXT("GameplayAbility_CharacterJump"), TEXT("GAJ_"));
    TestEqual(TEXT("最近祖先配置优先于较远领域基类"), Settings->ResolveAssetPrefix(Blueprint), FString(TEXT("GAJ_")));
    Settings->AssetClassPrefixes.Add(TEXT("/Script/GameplayAbilities.GameplayAbility_CharacterJump"), TEXT("GAJP_"));
    TestEqual(TEXT("同一祖先的完整路径配置优先于短类名"), Settings->ResolveAssetPrefix(Blueprint), FString(TEXT("GAJP_")));
    Blueprint->ParentClass = EffectClass;
    TestEqual(TEXT("效果蓝图采用领域GE前缀"), Settings->ResolveAssetPrefix(Blueprint), FString(TEXT("GE_")));
    Settings->AssetClassPrefixes.Add(TEXT("GameplayEffect"), TEXT("Effect_"));
    TestEqual(TEXT("领域前缀读取配置覆盖值"), Settings->ResolveAssetPrefix(Blueprint), FString(TEXT("Effect_")));
    Blueprint->ParentClass = AActor::StaticClass();
    TestEqual(TEXT("普通Actor蓝图保留BP前缀"), Settings->ResolveAssetPrefix(Blueprint), FString(TEXT("BP_")));
    Blueprint->ParentClass = nullptr;
    TestEqual(TEXT("尚未指定父类的普通蓝图保留BP前缀"), Settings->ResolveAssetPrefix(Blueprint), FString(TEXT("BP_")));

    UAnimBlueprint* AnimationBlueprint = NewObject<UAnimBlueprint>();
    AnimationBlueprint->ParentClass = UAnimInstance::StaticClass();
    Settings->AssetClassPrefixes.Add(TEXT("AnimInstance"), TEXT("Parent_"));
    TestEqual(TEXT("动画蓝图资产类型优先于父类规则"), Settings->ResolveAssetPrefix(AnimationBlueprint), FString(TEXT("ABP_")));
    UBlueprint* WidgetBlueprint = NewObject<UBlueprint>(GetTransientPackage(), WidgetBlueprintClass);
    WidgetBlueprint->ParentClass = UObject::StaticClass();
    Settings->AssetClassPrefixes.Add(TEXT("Object"), TEXT("Parent_"));
    TestEqual(TEXT("Widget蓝图资产类型优先于父类规则"), Settings->ResolveAssetPrefix(WidgetBlueprint), FString(TEXT("WBP_")));
    TestEqual(TEXT("空资产不生成虚构命名要求"), Settings->ResolveAssetPrefix(nullptr), FString());
    return true;
}

#endif

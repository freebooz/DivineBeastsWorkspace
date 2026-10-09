#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"

/** 测试项目 UI 定义与授权视图的身份、空资源和合法类边界，不伪造真实线上技能。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsAbilityUIProfileValidationTest,
    "DivineBeasts.UI.AbilityProfile.Validation",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsAbilityUIProfileValidationTest::RunTest(const FString&)
{
    UDivineBeastsAbilityUIProfile* Profile = NewObject<UDivineBeastsAbilityUIProfile>();
    FString Error;
    TestFalse(TEXT("空技能表现配置拒绝"), Profile->ValidateProfile(Error));
    Profile->HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    FDivineBeastsAbilityUIEntry Entry;
    Entry.AbilityId = TEXT("dba.ability.rat_example@1");
    Entry.DisplayName = FText::FromString(TEXT("测试技能，不是正式生产名称"));
    Profile->Entries.Add(Entry);
    TestFalse(TEXT("正式条目缺失图标须拒绝"), Profile->ValidateProfile(Error));
    // 仅测试用非空软路径，用于先通过缺图标门禁再单独验证重复 ID；不生成虚假 .uasset。
    Profile->Entries[0].Icon = TSoftObjectPtr<UTexture2D>(
        FSoftObjectPath(TEXT("/Game/Development/Tests/T_AbilityIconTest.T_AbilityIconTest")));
    Profile->Entries.Add(Entry);
    TestFalse(TEXT("重复稳定 AbilityId 被拒绝"), Profile->ValidateProfile(Error));
    return true;
}
#endif

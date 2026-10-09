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
    // 不依赖真实技能资源的最小负向UI安全验证：没有网络授权源时不暴露技能、
    // 不因构造一个ViewModel就虚构名称、冷却、等级或图标。
    UDivineBeastsAbilityBarViewModel* ViewModel =
        NewObject<UDivineBeastsAbilityBarViewModel>();
    TestNotNull(TEXT("技能栏视图模型可实例化"), ViewModel);
    if (!ViewModel)
    {
        return false;
    }
    TestFalse(TEXT("没有技能装配组件时不能绑定授权来源"),
        ViewModel->BindToLoadout(nullptr));
    TestEqual(TEXT("无授权时通用技能槽为空"), ViewModel->GetSlots().Num(), 0);
    TestEqual(TEXT("无授权时专属技能说明为空"), ViewModel->GetSlotDetails().Num(), 0);
    FDivineBeastsAbilitySlotDetails Detail;
    TestTrue(TEXT("默认技能显示身份为空"), Detail.AbilityId.IsNone());
    TestEqual(TEXT("默认冷却剩余秒数为0"), Detail.CooldownRemainingSeconds, 0.0f);
    ViewModel->UnbindFromLoadout();
    return true;
}
#endif

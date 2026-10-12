#if WITH_DEV_AUTOMATION_TESTS
// 项目层客户端回归：验证技能表现数据和创建页介绍所需的真实英雄文本来源。
// 由UE Automation调用；仅创建Transient测试对象或只读加载既有资产，不发HTTP、不授予技能、
// 不保存资产。资源强引用限于用例生命周期；这不是Widget/VM异步租约或运行画面验收。
#include "Misc/AutomationTest.h"
#include "Definitions/DivineBeastsAbilityUIProfile.h"
#include "Definitions/DivineBeastsHeroDefinition.h"
#include "ViewModels/Combat/DivineBeastsAbilityBarViewModel.h"
#include "Components/DivineBeastsAbilityLoadoutComponent.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

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
    Entry.Description = FText::FromString(TEXT("测试说明，使缺图标/重复编号断言分别保持单一失败原因。"));
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

/**
 * 介绍来源回归：读取十二份已保存HeroDefinition，要求存在可本地化且非空白的Introduction。
 * 反射访问让本测试可在尚未新增生产字段的主线上编译，由UE运行时给出明确RED，
 * 而不是用不存在的C++成员制造编译错误。字段存在后仍须逐份加载，CDO默认值不算作者内容。
 * 删除字段、遗漏任一英雄作者数据或写入空白介绍都会失败；不校验文案措辞，不修改定义身份。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHeroIntroductionDataAssetsTest,
    "DivineBeasts.UI.Characters.HeroOverview.HeroIntroductionDataAssets",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsHeroIntroductionDataAssetsTest::RunTest(const FString&)
{
    const FTextProperty* IntroductionProperty = FindFProperty<FTextProperty>(
        UDivineBeastsHeroDefinition::StaticClass(), TEXT("Introduction"));
    if (!TestNotNull(TEXT("HeroDefinition必须提供可本地化的Introduction来源字段"), IntroductionProperty))
    {
        return false;
    }

    // 类默认值保持空，保证逐份资产非空不是依赖一个写死的通用介绍通过门禁。
    const FText DefaultIntroduction = IntroductionProperty->GetPropertyValue_InContainer(
        GetDefault<UDivineBeastsHeroDefinition>());
    TestTrue(TEXT("原生类默认值不能充当十二英雄的作者介绍"),
        DefaultIntroduction.ToString().TrimStartAndEnd().IsEmpty());

    // 独立列出已交付英雄包名，避免由被测Catalog生成期望，遗漏目录项时不会同步漏测。
    const TCHAR* HeroSuffixes[] = {
        TEXT("Rat"), TEXT("Ox"), TEXT("Tiger"), TEXT("Rabbit"), TEXT("Dragon"), TEXT("Snake"),
        TEXT("Horse"), TEXT("Goat"), TEXT("Monkey"), TEXT("Rooster"), TEXT("Dog"), TEXT("Boar")
    };
    for (const TCHAR* Suffix : HeroSuffixes)
    {
        const FString AssetPath = FString::Printf(
            TEXT("/DBAGameplay/Definitions/DA_Hero_Zodiac_%s.DA_Hero_Zodiac_%s"), Suffix, Suffix);
        // 同步加载限于只读编辑器回归，运行时介绍VM必须使用GamePlatformData作用域租约。
        const TStrongObjectPtr<UDivineBeastsHeroDefinition> Definition(
            LoadObject<UDivineBeastsHeroDefinition>(nullptr, *AssetPath));
        if (!TestNotNull(FString::Printf(TEXT("真实英雄定义可读取：%s"), Suffix), Definition.Get()))
        {
            continue;
        }
        TestEqual(FString::Printf(TEXT("介绍来源保留真实英雄身份：%s"), Suffix),
            Definition->DefinitionId, FName(*FString::Printf(TEXT("Hero.Zodiac.%s"), Suffix)));
        const FText Introduction = IntroductionProperty->GetPropertyValue_InContainer(Definition.Get());
        TestFalse(FString::Printf(TEXT("已保存英雄介绍不能缺失或只有空白：%s"), Suffix),
            Introduction.ToString().TrimStartAndEnd().IsEmpty());
    }
    return true;
}

/**
 * 技能介绍缺失回归：先用完整作者文本证明配置本身合法，再只清空Description。
 * 回归曾遗漏的说明校验；先保持其他字段合法，再证明缺失说明被拒绝，不能用图标或名称替代说明。
 * 软图标路径仅用于静态字段验证，不进行加载，不把测试图标声明为真实交付资源。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHeroOverviewAbilityDescriptionRequiredTest,
    "DivineBeasts.UI.Characters.HeroOverview.AbilityDescriptionRequired",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsHeroOverviewAbilityDescriptionRequiredTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UDivineBeastsAbilityUIProfile> Profile(NewObject<UDivineBeastsAbilityUIProfile>());
    Profile->HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    FDivineBeastsAbilityUIEntry Entry;
    Entry.AbilityId = TEXT("dba.ability.rat_dev_primary@1");
    Entry.DisplayName = FText::FromString(TEXT("测试作者技能名称"));
    Entry.Description = FText::FromString(TEXT("测试作者说明，仅验证介绍数据完整性，不表示正式技能获批。"));
    Entry.Icon = TSoftObjectPtr<UTexture2D>(
        FSoftObjectPath(TEXT("/Game/Development/Tests/T_AbilityIconTest.T_AbilityIconTest")));
    Profile->Entries.Add(Entry);
    FString Error;
    if (!TestTrue(TEXT("完整作者文本通过静态配置验证"), Profile->ValidateProfile(Error)))
    {
        return false;
    }
    Profile->Entries[0].Description = FText::GetEmpty();
    TestFalse(TEXT("缺少技能说明不能被标记为可发布介绍配置"), Profile->ValidateProfile(Error));
    TestFalse(TEXT("缺少说明必须有可诊断原因"), Error.IsEmpty());
    return true;
}

/**
 * 空白说明回归：内容非空不等于玩家能看到介绍，空格/换行/制表符必须按缺失处理。
 * 先验证同一身份的可读说明，再替换为纯空白，避免把其他字段错误当作此回归通过的原因。
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsHeroOverviewAbilityDescriptionWhitespaceTest,
    "DivineBeasts.UI.Characters.HeroOverview.AbilityDescriptionWhitespaceRejected",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsHeroOverviewAbilityDescriptionWhitespaceTest::RunTest(const FString&)
{
    const TStrongObjectPtr<UDivineBeastsAbilityUIProfile> Profile(NewObject<UDivineBeastsAbilityUIProfile>());
    Profile->HeroDefinitionId = TEXT("Hero.Zodiac.Rat");
    FDivineBeastsAbilityUIEntry Entry;
    Entry.AbilityId = TEXT("dba.ability.rat_dev_primary@1");
    Entry.DisplayName = FText::FromString(TEXT("测试作者技能名称"));
    Entry.Description = FText::FromString(TEXT("非空白的测试作者说明。"));
    Entry.Icon = TSoftObjectPtr<UTexture2D>(
        FSoftObjectPath(TEXT("/Game/Development/Tests/T_AbilityIconTest.T_AbilityIconTest")));
    Profile->Entries.Add(Entry);
    FString Error;
    if (!TestTrue(TEXT("可读说明的基准配置通过"), Profile->ValidateProfile(Error)))
    {
        return false;
    }
    Profile->Entries[0].Description = FText::FromString(TEXT(" \t\r\n "));
    TestFalse(TEXT("只有空白的技能说明必须拒绝"), Profile->ValidateProfile(Error));
    TestFalse(TEXT("空白说明必须有可诊断原因"), Error.IsEmpty());
    return true;
}
#endif

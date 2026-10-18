// 项目客户端输入合同回归：校验技能语义、通道及本地玩家作用域，只读默认配置，不创建运行时绑定。
#include "Definitions/DivineBeastsInputProfileDefinition.h"
#include "Input/DivineBeastsInputSemantics.h"
#include "Misc/AutomationTest.h"
#include "Services/GamePlatformInputServices.h"
#include "Settings/DivineBeastsInputSettings.h"
#include "Subsystems/DivineBeastsInputClientSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsInputSemanticContractTest,
    "DivineBeasts.Input.SemanticContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsInputSemanticContractTest::RunTest(const FString&)
{
    const FGameplayTag PlatformMove =
        GamePlatformInputServices::GetBuiltInSemanticTag(EGamePlatformBuiltInInputSemantic::Move);
    TestTrue(TEXT("项目必须复用平台Built-in Move语义"), PlatformMove.IsValid());

    const FGameplayTag Tags[] =
    {
        DivineBeastsInputSemantics::AttackPrimary(),
        DivineBeastsInputSemantics::AbilitySlot1(),
        DivineBeastsInputSemantics::AbilitySlot2(),
        DivineBeastsInputSemantics::AbilitySlot3(),
        DivineBeastsInputSemantics::AbilitySlot4(),
        DivineBeastsInputSemantics::AbilitySlot5(),
        DivineBeastsInputSemantics::AbilitySlot6(),
        DivineBeastsInputSemantics::AbilitySlot7(),
        DivineBeastsInputSemantics::TargetLock()
    };

    TestEqual(TEXT("八个可使用技能槽加锁定为九种项目输入语义"),
        static_cast<int32>(UE_ARRAY_COUNT(Tags)), 9);
    TSet<FGameplayTag> Unique;
    for (const FGameplayTag Tag : Tags)
    {
        TestTrue(TEXT("项目输入Tag必须有效"), Tag.IsValid());
        TestFalse(TEXT("项目输入Tag必须唯一"), Unique.Contains(Tag));
        Unique.Add(Tag);

        const FGamePlatformInputSemanticDescriptor Descriptor =
            DivineBeastsInputSemantics::MakeGameplayActionDescriptor(Tag);
        TestTrue(
            TEXT("项目输入Descriptor必须满足平台通用合同"),
            GamePlatformInputServices::IsValidSemanticDescriptor(Descriptor));
        TestEqual(
            TEXT("项目输入使用Gameplay Action通道"),
            Descriptor.ChannelMask,
            static_cast<uint8>(EGamePlatformInputChannel::Actions));

        if (Tag != DivineBeastsInputSemantics::TargetLock())
        {
            const FGameplayTag AbilityTag = DivineBeastsInputSemantics::ToAbilityInputTag(Tag);
            TestTrue(TEXT("攻击/技能语义具有稳定Ability Input标签"), AbilityTag.IsValid());
            TestTrue(
                TEXT("Ability Input标签位于平台约定根下"),
                AbilityTag.ToString().StartsWith(TEXT("Platform.Ability.Input.DivineBeasts.")));
        }
    }
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsInputCompositionContractTest,
    "DivineBeasts.Input.CompositionContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsInputCompositionContractTest::RunTest(const FString&)
{
    TestTrue(TEXT("项目输入适配必须保持LocalPlayer作用域"),
        UDivineBeastsInputClientSubsystem::StaticClass()->IsChildOf(ULocalPlayerSubsystem::StaticClass()));

    const UDivineBeastsInputSettings* Settings = GetDefault<UDivineBeastsInputSettings>();
    TestNotNull(TEXT("神兽联盟输入项目设置必须存在"), Settings);
    TestTrue(TEXT("源码默认不得伪造不存在的Input Profile资产"),
        Settings && !Settings->DefaultGameplayProfileId.IsValid());
    TestEqual(TEXT("默认项目输入设置键稳定"),
        Settings ? Settings->LocalSettingsKey : FString(),
        FString(TEXT("DivineBeasts.Default")));
    return true;
}

#endif

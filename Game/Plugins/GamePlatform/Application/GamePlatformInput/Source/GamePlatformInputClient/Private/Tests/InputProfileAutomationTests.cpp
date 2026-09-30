// 平台输入Profile自动化测试：仅创建瞬态夹具，验证身份、动作合同与资源边界。
// 测试标签也必须延迟到引擎初始化后的游戏线程读取，测试源码会链接进Development单体Client。
#include "Definitions/GamePlatformInputProfileDefinition.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "GameplayTagsManager.h"
#include "Services/GamePlatformInputServices.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// 测试配置拥有此标签；只读缓存不在CRT初始化时访问UObject管理器。
static FGameplayTag GetInputAutomationCustom()
{
    check(IsInGameThread());
    static const FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Platform.Test.Input.Custom"), true);
    return Tag;
}

namespace
{
/**
 * 创建仅用于自动化校验的瞬态 Input Profile（输入配置）。
 * 不保存磁盘、不伪造正式 .uasset，只验证Definition字段和基础约束。
 */
UGamePlatformInputProfileDefinition* CreateValidInputProfile()
{
    UGamePlatformInputProfileDefinition* Profile =
        NewObject<UGamePlatformInputProfileDefinition>(
            GetTransientPackage(),
            NAME_None,
            RF_Transient);

    Profile->LogicalId.Namespace = TEXT("platform.input");
    Profile->LogicalId.Name = TEXT("automation");
    Profile->LogicalId.LogicalVersion = 1;

    auto AddAction = [Profile](
        EGamePlatformInputSemantic Semantic,
        EGamePlatformInputUnit Unit,
        EInputActionValueType ValueType,
        const TCHAR* Name)
    {
        UInputAction* Action = NewObject<UInputAction>(
            GetTransientPackage(),
            FName(Name),
            RF_Transient);
        Action->ValueType = ValueType;

        FGamePlatformInputActionDefinition Entry;
        Entry.Semantic = Semantic;
        Entry.Unit = Unit;
        Entry.Action = Action;
        Profile->Actions.Add(Entry);
    };

    AddAction(
        EGamePlatformInputSemantic::Move,
        EGamePlatformInputUnit::NormalizedAxis,
        EInputActionValueType::Axis2D,
        TEXT("IA_AutomationMove"));
    AddAction(
        EGamePlatformInputSemantic::Menu,
        EGamePlatformInputUnit::Boolean,
        EInputActionValueType::Boolean,
        TEXT("IA_AutomationMenu"));
    AddAction(
        EGamePlatformInputSemantic::Cancel,
        EGamePlatformInputUnit::Boolean,
        EInputActionValueType::Boolean,
        TEXT("IA_AutomationCancel"));

    UInputMappingContext* Context = NewObject<UInputMappingContext>(
        GetTransientPackage(),
        TEXT("IMC_Automation"),
        RF_Transient);

    FGamePlatformInputContextDefinition ContextEntry;
    ContextEntry.Name = TEXT("Gameplay");
    ContextEntry.Context = Context;
    Profile->Contexts.Add(ContextEntry);

    return Profile;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInputProfileContractTest,
    "GamePlatform.Input.Profile.Contract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformInputProfileContractTest::RunTest(const FString&)
{
    UGamePlatformInputProfileDefinition* Profile = CreateValidInputProfile();
    TestNotNull(TEXT("瞬态输入Profile创建成功"), Profile);
    if (!Profile) { return false; }

    TestTrue(TEXT("默认PC+手柄+Touch配置合法"), Profile->ValidateDefinition().IsSuccess());

    Profile->bEnableKeyboardMouse = false;
    Profile->bEnableGamepad = false;
    Profile->bEnableTouch = false;
    TestFalse(TEXT("至少必须启用一个设备族"), Profile->ValidateDefinition().IsSuccess());

    Profile->bEnableKeyboardMouse = true;
    Profile->TouchAnalogDeadZone = 0.75;
    TestFalse(TEXT("Touch死区越界被拒绝"), Profile->ValidateDefinition().IsSuccess());

    Profile->TouchAnalogDeadZone = 0.12;
    Profile->DefaultAccessibility.LookSensitivityMultiplier = 8.0;
    TestFalse(TEXT("无障碍视角灵敏度越界被拒绝"), Profile->ValidateDefinition().IsSuccess());

    Profile->DefaultAccessibility.LookSensitivityMultiplier = 1.0;
    Profile->DefaultAccessibility.TouchLookSensitivityMultiplier = 5.0;
    TestFalse(TEXT("Touch视角灵敏度越界被拒绝"), Profile->ValidateDefinition().IsSuccess());

    Profile->DefaultAccessibility.TouchLookSensitivityMultiplier = 1.0;
    Profile->DefaultAccessibility.TouchMoveScale = 2.0;
    TestFalse(TEXT("Touch移动倍率越界被拒绝"), Profile->ValidateDefinition().IsSuccess());

    Profile->DefaultAccessibility.TouchMoveScale = 1.0;
    Profile->Actions[0].Unit = EGamePlatformInputUnit::Boolean;
    TestFalse(TEXT("Move语义与单位不匹配被拒绝"), Profile->ValidateDefinition().IsSuccess());

    // 新语义Descriptor不依赖固定枚举，并可在同一Profile中与Legacy动作共存。
    FGamePlatformInputActionDefinition CustomEntry;
    CustomEntry.Descriptor.SemanticId.Tag = GetInputAutomationCustom();
    CustomEntry.Descriptor.Unit = EGamePlatformInputUnit::Boolean;
    CustomEntry.Descriptor.ValueType = EInputActionValueType::Boolean;
    CustomEntry.Descriptor.ChannelMask = static_cast<uint8>(EGamePlatformInputChannel::Actions);
    CustomEntry.Descriptor.ValuePolicy = EGamePlatformInputValuePolicy::Passthrough;
    CustomEntry.Action = NewObject<UInputAction>(GetTransientPackage(), TEXT("IA_AutomationCustom"), RF_Transient);
    CustomEntry.Action.Get()->ValueType = EInputActionValueType::Boolean;
    Profile->Actions[0].Unit = EGamePlatformInputUnit::NormalizedAxis;
    Profile->Actions.Add(CustomEntry);
    TestTrue(TEXT("自定义Tag语义可与旧枚举Profile兼容共存"), Profile->ValidateDefinition().IsSuccess());

    CustomEntry.Descriptor.ChannelMask = 0;
    Profile->Actions.Last() = CustomEntry;
    TestFalse(TEXT("自定义Tag语义的空阻断通道被拒绝"), Profile->ValidateDefinition().IsSuccess());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformBuiltInInputSemanticContractTest,
    "GamePlatform.Input.Semantic.BuiltInContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformBuiltInInputSemanticContractTest::RunTest(const FString&)
{
    const EGamePlatformBuiltInInputSemantic BuiltIns[] =
    {
        EGamePlatformBuiltInInputSemantic::Move,
        EGamePlatformBuiltInInputSemantic::LookDelta,
        EGamePlatformBuiltInInputSemantic::LookRate,
        EGamePlatformBuiltInInputSemantic::Interact,
        EGamePlatformBuiltInInputSemantic::Menu,
        EGamePlatformBuiltInInputSemantic::Confirm,
        EGamePlatformBuiltInInputSemantic::Cancel
    };

    TSet<FGameplayTag> UniqueTags;
    for (const EGamePlatformBuiltInInputSemantic Semantic : BuiltIns)
    {
        const FGameplayTag Tag = GamePlatformInputServices::GetBuiltInSemanticTag(Semantic);
        const FGamePlatformInputSemanticDescriptor Descriptor =
            GamePlatformInputServices::GetBuiltInSemanticDescriptor(Semantic);
        TestTrue(TEXT("平台Built-in语义Tag必须有效"), Tag.IsValid());
        TestFalse(TEXT("平台Built-in语义Tag必须唯一"), UniqueTags.Contains(Tag));
        UniqueTags.Add(Tag);
        TestEqual(TEXT("Built-in Descriptor与Tag必须一致"), Descriptor.SemanticId.Tag, Tag);
        TestTrue(
            TEXT("平台Built-in Descriptor必须满足通用运行合同"),
            GamePlatformInputServices::IsValidSemanticDescriptor(Descriptor));
    }

    TestEqual(
        TEXT("平台Built-in集合只能包含真正跨游戏公共语义"),
        UniqueTags.Num(),
        7);
    return true;
}

#endif

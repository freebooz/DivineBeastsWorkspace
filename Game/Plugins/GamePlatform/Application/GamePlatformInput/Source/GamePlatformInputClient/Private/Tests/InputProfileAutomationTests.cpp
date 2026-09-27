#include "Definitions/GamePlatformInputProfileDefinition.h"

#include "InputAction.h"
#include "InputMappingContext.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

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

    return true;
}

#endif

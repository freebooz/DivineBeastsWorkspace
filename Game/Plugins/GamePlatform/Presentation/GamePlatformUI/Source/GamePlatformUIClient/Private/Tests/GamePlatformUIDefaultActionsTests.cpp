// 第一层UI启动资产回归：只读取平台中立CommonInput数据及真实表行，不认识项目页面或Gameplay键位。
// Editor/Client开发自动化可运行；不激活页面、不注入设备事件，不能替代真实ESC/手柄人工验证。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CommonInputBaseTypes.h"
#include "CommonUITypes.h"
#include "InputCoreTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGamePlatformUIDefaultActionsTest,
    "GamePlatform.UI.Input.DefaultActionsAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FGamePlatformUIDefaultActionsTest::RunTest(const FString&)
{
    UClass* InputClass=LoadClass<UCommonUIInputData>(nullptr,
        TEXT("/GamePlatformUI/Input/BP_GP_UIInputData.BP_GP_UIInputData_C"));
    if(!TestNotNull(TEXT("真实平台输入蓝图必须可加载"),InputClass)) return false;
    const auto* Data=InputClass->GetDefaultObject<UCommonUIInputData>();
    TestFalse(TEXT("UI输入数据不进入专用服务器"),Data->NeedsLoadForServer());
    const auto* Back=Data->DefaultBackAction.GetRow<FCommonInputActionDataBase>(TEXT("UI Back asset validation"));
    const auto* Accept=Data->DefaultClickAction.GetRow<FCommonInputActionDataBase>(TEXT("UI Accept asset validation"));
    if(!TestNotNull(TEXT("返回不能指向空DataTable行"),Back) || !TestNotNull(TEXT("确认不能指向空DataTable行"),Accept)) return false;
    TestEqual(TEXT("平台键盘返回为ESC"),Back->GetInputTypeInfo(ECommonInputType::MouseAndKeyboard,NAME_None).GetKey(),EKeys::Escape);
    TestEqual(TEXT("平台键盘确认为Enter"),Accept->GetInputTypeInfo(ECommonInputType::MouseAndKeyboard,NAME_None).GetKey(),EKeys::Enter);
    TestEqual(TEXT("平台手柄返回键"),Back->GetInputTypeInfo(ECommonInputType::Gamepad,NAME_None).GetKey(),EKeys::Gamepad_FaceButton_Right);
    return true;
}
#endif

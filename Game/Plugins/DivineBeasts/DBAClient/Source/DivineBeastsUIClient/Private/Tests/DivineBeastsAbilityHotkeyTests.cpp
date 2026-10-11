// 项目客户端技能快捷键格式回归：只检查真实映射的显示转换，不授予技能、不创建Widget或修改输入配置。
// 由UE开发自动化运行；未映射和被动槽必须留空，不能用固定数字制造不存在的绑定。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAbilityHotkeyTest,
    "DivineBeasts.UI.Combat.AbilityHotkeyMapping",
    EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsAbilityHotkeyTest::RunTest(const FString&)
{
    // 使用独立映射夹具而非全局InputSettings，测试执行不能改变正在运行玩家的真实按键。
    TArray<FInputActionKeyMapping> Mappings;
    TestTrue(TEXT("没有实际映射时不伪造按键"),
        UDivineBeastsAbilityBarPanel::FormatHotkeyMappings(Mappings, false).IsEmpty());
    Mappings.Emplace(TEXT("DBA.PrimaryAbility"), EKeys::One);
    TestEqual(TEXT("无修饰键使用引擎按键显示名称"),
        UDivineBeastsAbilityBarPanel::FormatHotkeyMappings(Mappings, false).ToString(),
        EKeys::One.GetDisplayName().ToString());
    TestTrue(TEXT("被动槽即使误收到映射也必须无按键"),
        UDivineBeastsAbilityBarPanel::FormatHotkeyMappings(Mappings, true).IsEmpty());

    Mappings.Reset();
    // 构造参数依次为Shift/Ctrl/Alt/Cmd；呈现顺序固定为Ctrl/Alt/Shift/Cmd便于识别。
    Mappings.Emplace(TEXT("DBA.AbilityOne"), EKeys::Two, true, true, true, true);
    const FString ModifiedKey = FString(TEXT("Ctrl+Alt+Shift+Cmd+")) + EKeys::Two.GetDisplayName().ToString();
    TestEqual(TEXT("每个真实修饰键均参与显示"),
        UDivineBeastsAbilityBarPanel::FormatHotkeyMappings(Mappings, false).ToString(), ModifiedKey);
    Mappings.Emplace(TEXT("DBA.AbilityOne"), EKeys::Two, true, true, true, true);
    Mappings.Emplace(TEXT("DBA.AbilityOne"), EKeys::Three);
    Mappings.Emplace(TEXT("DBA.AbilityOne"), EKeys::Invalid);
    Mappings.Emplace(TEXT("DBA.AbilityOne"), EKeys::Gamepad_FaceButton_Bottom);
    Mappings.Emplace(TEXT("DBA.AbilityOne"), EKeys::TouchKeys[0]);
    TestEqual(TEXT("保留多个实际键、去重，忽略无效键和非PC按键"),
        UDivineBeastsAbilityBarPanel::FormatHotkeyMappings(Mappings, false).ToString(),
        ModifiedKey + TEXT(" / ") + EKeys::Three.GetDisplayName().ToString());
    return true;
}
#endif

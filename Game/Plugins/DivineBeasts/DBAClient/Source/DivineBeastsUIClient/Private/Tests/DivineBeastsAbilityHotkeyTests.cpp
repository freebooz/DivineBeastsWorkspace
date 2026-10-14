// 项目客户端技能输入回归：检查实际映射提示和已保存技能槽的鼠标入口，不授予技能或修改玩家输入配置。
// 由UE开发自动化运行；指针用例创建独立游戏实例与真实Widget，结束时释放，不访问业务后端。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Panels/Combat/DivineBeastsAbilityBarPanel.h"
#include "GameFramework/PlayerInput.h"
#include "InputCoreTypes.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/ScopeExit.h"
#include "Widgets/SWidget.h"
#include "Input/Events.h"

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
/** 实际已授权技能槽若不处理左键，鼠标点击永远不能到达后续视图模型命令。 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsAbilitySlotPointerTest,
    "DivineBeasts.UI.Combat.AbilitySlotPointer.Invocation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsAbilitySlotPointerTest::RunTest(const FString&)
{
    // InitializeStandalone要求外层为实际Engine；默认Transient Package会触发CastChecked致命错误。
    // 仍使用独立实例与唯一世界身份，不初始化/替换正在运行玩家的GameInstance。
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>(GEngine));
    Instance->InitializeStandalone(FName(*FGuid::NewGuid().ToString()));
    UWorld* FixtureWorld = Instance->GetWorld();
    ON_SCOPE_EXIT
    {
        // Shutdown只清实例的WorldContext指针，不删除Engine中该上下文或InitializeStandalone创建的根世界。
        // 先解绑本夹具子系统，再精确销毁自己的上下文/世界，不留下后续UI测试可见的游戏世界。
        Instance->Shutdown();
        if (FixtureWorld)
        {
            GEngine->DestroyWorldContext(FixtureWorld);
            FixtureWorld->DestroyWorld(false);
        }
    };
    UClass* SlotClass = LoadClass<UGamePlatformSlotWidget>(nullptr,
        TEXT("/DBAUIPack_Core/UI/Combat/WBP_DBA_UI_AbilitySlot.WBP_DBA_UI_AbilitySlot_C"));
    if (!TestNotNull(TEXT("真实公共技能槽资产存在"), SlotClass)) { return false; }
    UGamePlatformSlotWidget* Slot = CreateWidget<UGamePlatformSlotWidget>(Instance.Get(), SlotClass);
    if (!TestNotNull(TEXT("真实技能槽成功初始化"), Slot)) { return false; }
    TStrongObjectPtr<UGamePlatformSlotWidget> SlotOwner(Slot);
    FGamePlatformUISlotState State;
    State.SlotId = TEXT("Platform.Ability.Input.DivineBeasts.Slot1");
    State.ContentId = TEXT("dba.ability.rat_dev_fireball@1");
    State.bEnabled = true;
    Slot->ApplySlotState(State);
    const TSet<FKey> Pressed{EKeys::LeftMouseButton};
    const FPointerEvent Pointer(0, FVector2D(32.f, 32.f), FVector2D(32.f, 32.f),
        Pressed, EKeys::LeftMouseButton, 0.f, FModifierKeysState());
    const FGeometry Geometry = FGeometry::MakeRoot(FVector2D(64.f, 64.f), FSlateLayoutTransform());
    const TSharedRef<SWidget> SlateSlot = Slot->TakeWidget();
    TestTrue(TEXT("已授权技能槽左键进入交互命令入口"), SlateSlot->OnMouseButtonDown(Geometry, Pointer).IsEventHandled());
    State.bPending = true;
    Slot->ApplySlotState(State);
    TestFalse(TEXT("待确认授权不能受理鼠标施法"), SlateSlot->OnMouseButtonDown(Geometry, Pointer).IsEventHandled());
    return true;
}
#endif

// 第三层公共面板真实资产回归：读取保存蓝图/共享CommonBorderStyle、原生Box九宫格与配置返回动作。
// 无激活临时实例仅用于验证父类、内容插槽和转换后的Canvas锚点；不操作账号、设备设置或网络。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CommonBorder.h"
#include "CommonInputSettings.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/NamedSlot.h"
#include "Materials/MaterialInterface.h"
#include "Panels/DivineBeastsPanelWidget.h"
#include "Screens/System/DivineBeastsSystemMenuScreen.h"
#include "Screens/System/DivineBeastsDeviceSettingsScreen.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPanelStyleAssetTest,
    "DivineBeasts.UI.Style.PanelNineSliceAsset",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsPanelStyleAssetTest::RunTest(const FString&)
{
    UClass* StyleClass=LoadClass<UCommonBorderStyle>(nullptr,TEXT("/DBAUIPack_Core/UI/Styles/BP_DBA_BorderStyle_Panel.BP_DBA_BorderStyle_Panel_C"));
    if(!TestNotNull(TEXT("公共样式类可从真实资产加载"),StyleClass)) return false;
    FSlateBrush Brush;
    StyleClass->GetDefaultObject<UCommonBorderStyle>()->GetBackgroundBrush(Brush);
    TestTrue(TEXT("九宫格为原生Box而不是整图缩放"),Brush.DrawAs==ESlateBrushDrawType::Box);
    TestTrue(TEXT("左边角比例已保存"),FMath::IsNearlyEqual(Brush.Margin.Left,.16f));
    TestTrue(TEXT("上边角比例已保存"),FMath::IsNearlyEqual(Brush.Margin.Top,.22f));
    TestTrue(TEXT("样式资源来自本次原稿材质"),Brush.GetResourceObject() && Brush.GetResourceObject()->GetPathName().Contains(TEXT("M_DBA_NaturePanelNineSlice")));
    UClass* PanelClass=LoadClass<UDivineBeastsPanelWidget>(nullptr,TEXT("/DBAUIPack_Core/UI/Components/WBP_DBA_UI_StandardPanel.WBP_DBA_UI_StandardPanel_C"));
    if(!TestNotNull(TEXT("现有标准面板真实类"),PanelClass)) return false;
    TStrongObjectPtr<UDivineBeastsPanelWidget> Panel(NewObject<UDivineBeastsPanelWidget>(GetTransientPackage(),PanelClass));
    if(!TestTrue(TEXT("标准面板能Initialize"),Panel->Initialize()))return false;
    TestNotNull(TEXT("背景复用CommonBorder能力"),Cast<UCommonBorder>(Panel->GetWidgetFromName(TEXT("PanelBackground"))));
    TestNotNull(TEXT("业务内容NamedSlot在转换后仍保留"),Cast<UNamedSlot>(Panel->GetWidgetFromName(TEXT("ContentSlot"))));
    // 转换工具曾丢失Canvas位置，必须读取保存实例验证；不能把工具返回success当作布局证据。
    for(const TCHAR* Name:{TEXT("SystemMenu"),TEXT("Settings")})
    {
        const FString Path=FString::Printf(TEXT("/DBAUIPack_Core/UI/Screens/WBP_DBA_UI_%s.WBP_DBA_UI_%s_C"),Name,Name);
        UClass* ScreenClass=LoadClass<UUserWidget>(nullptr,*Path);
        if(!TestNotNull(TEXT("系统页真实类"),ScreenClass))continue;
        TStrongObjectPtr<UUserWidget> Screen(NewObject<UUserWidget>(GetTransientPackage(),ScreenClass));
        if(!TestTrue(TEXT("系统页Initialize"),Screen->Initialize()))continue;
        auto* Background=Cast<UCommonBorder>(Screen->GetWidgetFromName(TEXT("PanelBackground")));
        if(!TestNotNull(TEXT("系统页公共背景"),Background))continue;
        auto* Slot=Cast<UCanvasPanelSlot>(Background->Slot);
        if(!TestNotNull(TEXT("系统页背景Canvas插槽"),Slot))continue;
        TestEqual(TEXT("背景横向居中锚点"),Slot->GetAnchors().Minimum.X,.5);
        TestEqual(TEXT("背景纵向居中锚点"),Slot->GetAnchors().Minimum.Y,.5);
        TestEqual(TEXT("背景居中对齐"),Slot->GetAlignment(),FVector2D(.5,.5));
    }
    // 无窗口资产回归没有本地玩家输入子系统，不能假定其启动路径已加载CommonInput配置。
    // 显式执行引擎公开加载入口，再验证真实配置与保存的返回行；不注入测试替代输入数据。
    auto* InputSettings=GetMutableDefault<UCommonInputSettings>();
    InputSettings->LoadData();
    const auto Back=InputSettings->GetDefaultBackAction();
    TestNotNull(TEXT("项目配置必须真实装载默认返回表"),Back.DataTable.Get());
    TestEqual(TEXT("项目配置返回行"),Back.RowName,FName(TEXT("Back")));
    return true;
}
#endif

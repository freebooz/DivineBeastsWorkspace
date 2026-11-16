// 新资源参数与稳定标识严格分离；路径语法/输入回归不冒充原生资源加载或视听验收。
#include "Catalog/DivineBeastsDemoCatalogValidation.h"
#include "UI/DivineBeastsDemoParameterRow.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsDemoAssetParameterTest,
    "DivineBeasts.PluginDemos.AssetParameter", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsDemoAssetParameterTest::RunTest(const FString&)
{
    const FString Valid(TEXT("/DBAHeroPack_Rat/UI/T_Icon.T_Icon"));
    TestTrue(TEXT("实际顶层对象路径通过语法"), DivineBeasts::Demo::IsStableAssetPathText(Valid));
    TestFalse(TEXT("资源路径不会放宽稳定身份合同"), DivineBeasts::Demo::IsStableIdentityText(Valid));
    for (const TCHAR* Invalid : {TEXT("C:/Temp/Test.png"), TEXT("/Game/A.A:Inner"), TEXT("/Game/A.A;quit"), TEXT("/Game/A.A\n"),
        TEXT("/Game/../A.A"), TEXT("/Content/A.A"), TEXT("/Game/A"), TEXT("/Game/A.A/B"), TEXT("/Game//A.A")})
        TestFalse(FString(TEXT("拒绝非法路径：")) + Invalid, DivineBeasts::Demo::IsStableAssetPathText(Invalid));
    auto* Item = NewObject<UDivineBeastsDemoParameterItem>();
    Item->Definition.ParameterId = TEXT("Asset"); Item->Definition.DefaultValue.Type = EDivineBeastsDemoParameterType::AssetPath;
    Item->Definition.bWritable = true; Item->Definition.ApplyWhen = TEXT("execute");
    Item->Value = Item->Definition.DefaultValue;
    TestTrue(TEXT("原生参数行接受资源路径"), Item->SetTextValue(Valid));
    TestEqual(TEXT("路径载荷完整保留"), Item->Value.AssetPathValue.ToString(), Valid);
    TestFalse(TEXT("参数行拒绝脚本文本"), Item->SetTextValue(TEXT("/Game/A.A;quit")));
    TestEqual(TEXT("失败输入不改上一有效载荷"), Item->Value.AssetPathValue.ToString(), Valid);
    Item->Definition.bWritable = false;
    TestFalse(TEXT("只读资源参数不可编辑"), Item->SetTextValue(TEXT("/Game/B.B")));
    return true;
}
#endif

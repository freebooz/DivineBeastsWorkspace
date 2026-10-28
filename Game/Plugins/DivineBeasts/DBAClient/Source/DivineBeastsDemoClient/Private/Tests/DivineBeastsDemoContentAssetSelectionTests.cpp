// 内容选择原生合同回归；不写资源，不将路径验证当真实资产预览通过。
#include "Scenarios/DivineBeastsDemoContentAssetScenario.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsContentAssetSelectionTest,
    "DivineBeasts.PluginDemos.ContentAssetSelection", EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FDivineBeastsContentAssetSelectionTest::RunTest(const FString&)
{
    using namespace DivineBeasts::Demo;
    const FName Owner(TEXT("DBAHeroPack_Rat"));
    TestTrue(TEXT("本包顶层对象合法"), ValidateContentAssetSelection(Owner, FSoftObjectPath(TEXT("/DBAHeroPack_Rat/UI/T_Icon.T_Icon"))).IsSuccess());
    TestFalse(TEXT("空选择不启动"), ValidateContentAssetSelection(Owner, {}).IsSuccess());
    TestFalse(TEXT("空所有者不启动"), ValidateContentAssetSelection(NAME_None, FSoftObjectPath(TEXT("/DBAHeroPack_Rat/UI/T_Icon.T_Icon"))).IsSuccess());
    TestFalse(TEXT("不得使用另一个包的图标"), ValidateContentAssetSelection(Owner, FSoftObjectPath(TEXT("/DBAHeroPack_Ox/UI/T_Icon.T_Icon"))).IsSuccess());
    TestFalse(TEXT("相同前缀不是同一个挂载点"), ValidateContentAssetSelection(Owner, FSoftObjectPath(TEXT("/DBAHeroPack_RatExtra/UI/T_Icon.T_Icon"))).IsSuccess());
    TestFalse(TEXT("不操作资产内子对象"), ValidateContentAssetSelection(Owner, FSoftObjectPath(TEXT("/DBAHeroPack_Rat/UI/T_Icon.T_Icon:Inner"))).IsSuccess());
    TestFalse(TEXT("不接受类后缀代替Blueprint资产"), ValidateContentAssetSelection(Owner, FSoftObjectPath(TEXT("/DBAHeroPack_Rat/UI/WBP_Icon.WBP_Icon_C"))).IsSuccess());
    TestFalse(TEXT("不接受磁盘文件"), ValidateContentAssetSelection(Owner, FSoftObjectPath(TEXT("C:/Users/Test.png"))).IsSuccess());
    return true;
}
#endif

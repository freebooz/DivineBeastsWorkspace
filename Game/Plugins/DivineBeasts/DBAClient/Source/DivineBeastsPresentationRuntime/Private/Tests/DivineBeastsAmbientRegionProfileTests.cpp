// 神兽联盟区域环境视觉配置测试：只验证项目层权重选择，不播放或复制Niagara。
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Ambient/DivineBeastsAmbientRegionProfile.h"
// 非Unity编译必须包含UPackage完整定义，才能将GetTransientPackage转换为NewObject的Outer。
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDivineBeastsAmbientRegionProfileSelectionTest,
    "DivineBeasts.Presentation.AmbientNature.RegionProfile.Selection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsAmbientRegionProfileSelectionTest::RunTest(const FString&)
{
    UDivineBeastsAmbientRegionProfile* Profile =
        NewObject<UDivineBeastsAmbientRegionProfile>(GetTransientPackage());
    if (!TestNotNull(TEXT("区域配置对象存在"), Profile))
    {
        return false;
    }

    FDivineBeastsAmbientStyleWeight Pear;
    Pear.StyleId = TEXT("PearPetal");
    Pear.DefinitionId = TEXT("presentation.dba.environment.ambient.pearpetal@1");
    Pear.Weight = 0.25f;
    FDivineBeastsAmbientStyleWeight Cherry;
    Cherry.StyleId = TEXT("CherryPetal");
    Cherry.DefinitionId = TEXT("presentation.dba.environment.ambient.cherrypetal@1");
    Cherry.Weight = 0.75f;
    Profile->Styles = {Pear, Cherry};
    TestEqual(TEXT("归一化零点选择首个风格"),
        Profile->SelectStyle(0.f), FName(TEXT("PearPetal")));
    TestEqual(TEXT("首个风格区间内选择正确"),
        Profile->SelectStyle(0.24f), FName(TEXT("PearPetal")));
    TestEqual(TEXT("风格与平台定义身份必须选中同一权重区间"),
        Profile->SelectDefinitionId(0.24f),
        FName(TEXT("presentation.dba.environment.ambient.pearpetal@1")));
    TestEqual(TEXT("边界值落入下一个风格"),
        Profile->SelectStyle(0.25f), FName(TEXT("CherryPetal")));
    TestEqual(TEXT("定义身份也必须在同一边界切换"),
        Profile->SelectDefinitionId(0.25f),
        FName(TEXT("presentation.dba.environment.ambient.cherrypetal@1")));
    TestEqual(TEXT("1.0及超范围退化为最后有效风格"),
        Profile->SelectStyle(1.5f), FName(TEXT("CherryPetal")));

    Pear.Weight = -1.f;
    Profile->Styles = {Pear, Cherry};
    TestEqual(TEXT("负权重被排除"),
        Profile->SelectStyle(0.1f), FName(TEXT("CherryPetal")));

    Pear.StyleId = NAME_None;
    Cherry.Weight = 0.f;
    Profile->Styles = {Pear, Cherry};
    TestEqual(TEXT("无有效权重/身份时拒绝选择"),
        Profile->SelectStyle(0.5f), NAME_None);
    TestEqual(TEXT("缺失定义身份不得构造资源加载请求"),
        Profile->SelectDefinitionId(0.5f), NAME_None);

    Profile->Styles.Reset();
    TestEqual(TEXT("空区域预设拒绝选择"),
        Profile->SelectStyle(0.f), NAME_None);
    return !HasAnyErrors();
}
#endif

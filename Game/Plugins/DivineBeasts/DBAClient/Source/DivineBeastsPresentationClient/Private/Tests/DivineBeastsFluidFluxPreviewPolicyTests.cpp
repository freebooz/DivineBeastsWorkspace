// Fluid Flux可选客户端表现不需要第三方源码才能测试失败关闭路径。
#include "Water/DivineBeastsCharacterStudioWaterPolicy.h"
#include "Misc/AutomationTest.h"
#include "Misc/ConfigCacheIni.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsCharacterStudioWaterPolicyTests,
    "DivineBeasts.Presentation.CharacterStudio.Water.FluidFluxFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsCharacterStudioWaterPolicyTests::RunTest(const FString& Parameters)
{
    using EMode = EDivineBeastsCharacterStudioWaterMode;
    // 缺舞台时不允许启用可选流体；不依赖第三方插件是否安装，也不允许伪造预览关卡。
    TestTrue(TEXT("缺实际工作室世界必须回退原生水面"),
        UDivineBeastsCharacterStudioWaterPolicy::ResolveWaterMode(nullptr, true, true) == EMode::Native);
    TestTrue(TEXT("未申请Fluid Flux时保持原生"),
        UDivineBeastsCharacterStudioWaterPolicy::ResolveWaterMode(nullptr, false, true) == EMode::Native);
    TestTrue(TEXT("低画质时保持原生"),
        UDivineBeastsCharacterStudioWaterPolicy::ResolveWaterMode(nullptr, true, false) == EMode::Native);

    bool bVerified = false;
    const bool bApproved = GConfig && GConfig->GetBool(
        TEXT("DivineBeasts.CharacterStudio.Water"),
        TEXT("bFluidFlux3PreviewVerified"), bVerified, GGameIni) && bVerified;
    if (!bApproved)
    {
        // 不修改GConfig或用户机器配置；默认无审核许可时必须与插件安装状态无关。
        TestFalse(TEXT("缺失UE5.8审核许可时插件检测必须拒绝"),
            UDivineBeastsCharacterStudioWaterPolicy::IsFluidFluxPluginEligible());
    }
    else
    {
        AddInfo(TEXT("当前环境显式声明已验收；插件加载与场景表现仍由单独端到端测试验证。"));
    }
    return true;
}
#endif

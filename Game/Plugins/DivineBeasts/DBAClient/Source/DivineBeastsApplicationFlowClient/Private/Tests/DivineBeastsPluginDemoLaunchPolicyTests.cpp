// 开发模式纯判定回归：覆盖合法启动、发行/专服/命令工具、缺开关、错误地图及冲突。
// 不启动后端，不创建伪UE资产；真实运行/发行Cook需要后续独立验收。
#include "Development/DivineBeastsPluginDemoMode.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDivineBeastsPluginDemoPolicyTest,
    "DivineBeasts.PluginDemos.LaunchPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDivineBeastsPluginDemoPolicyTest::RunTest(const FString& Parameters)
{
    const FString Map = FDivineBeastsPluginDemoMode::GetHubMapPackage();
    auto Allowed = [](bool Dev, bool Client, bool OptIn, bool Conflict, const FString& Path)
    {
        return FDivineBeastsPluginDemoMode::IsAllowedArguments(Dev, Client, OptIn, Conflict, Path);
    };
    TestTrue(TEXT("开发交互客户端的明确总览入口允许"),Allowed(true,true,true,false,Map));
    const FString CameraReview = FDivineBeastsPluginDemoMode::GetCameraReviewMapPackage();
    TestTrue(TEXT("开发客户端显式电影镜头演示入口允许"), Allowed(true,true,true,false,CameraReview));
    TestFalse(TEXT("电影镜头演示在发行配置拒绝"), Allowed(false,true,true,false,CameraReview));
    TestFalse(TEXT("专用服务器电影镜头演示拒绝"), Allowed(true,false,true,false,CameraReview));
    TestFalse(TEXT("未经双参数许可的电影镜头演示拒绝"), Allowed(true,true,false,false,CameraReview));
    TestFalse(TEXT("电影镜头地图模糊匹配拒绝"), Allowed(true,true,true,false,CameraReview + TEXT("_Fake")));
    TestFalse(TEXT("电影镜头地图大小写不一致拒绝"), Allowed(true,true,true,false,CameraReview.ToLower()));    TestFalse(TEXT("发行配置拒绝"),Allowed(false,true,true,false,Map));
    TestFalse(TEXT("服务器和命令工具拒绝"),Allowed(true,false,true,false,Map));
    TestFalse(TEXT("未授权参数拒绝"),Allowed(true,true,false,false,Map));
    TestFalse(TEXT("Foundation开发模式冲突拒绝"),Allowed(true,true,true,true,Map));
    TestFalse(TEXT("空地图拒绝"),Allowed(true,true,true,false,TEXT("")));
    TestFalse(TEXT("正式登录地图拒绝"),Allowed(true,true,true,false,TEXT("/DBAFrontEndPack/Maps/L_DBA_FrontEnd")));
    TestFalse(TEXT("地图相似名称拒绝"),Allowed(true,true,true,false,Map + TEXT("_Fake")));
    TestFalse(TEXT("地图大小写不匹配拒绝"),Allowed(true,true,true,false,Map.ToLower()));
    return true;
}
#endif

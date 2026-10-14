// P0真实C++自动化：只校验启动模式纯规则，实际UI/登录/演示地图必须另作运行验证。
#include "Development/DivineBeastsDevelopmentLaunchMode.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDBADevelopmentLaunchDebugTest,
    "DivineBeasts.PluginDev.LaunchMode.DefaultAndExplicit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDBADevelopmentLaunchDebugTest::RunTest(const FString&)
{
    using Mode = EDBADevelopmentLaunchMode;
    TestEqual(TEXT("未提供启动参数时保持完整Debug登录"), FDBADevelopmentLaunchMode::Resolve(TEXT(""), false, false, false), Mode::Debug);
    TestEqual(TEXT("旧版单参数不得独立绕过登录"), FDBADevelopmentLaunchMode::Resolve(TEXT("-DBALaunchMode=PluginDev"), false, false, false), Mode::Debug);
    TestEqual(TEXT("缺少地图不得进入插件开发"), FDBADevelopmentLaunchMode::Resolve(TEXT("-DBAPluginDemo"), false, false, false), Mode::Debug);
    TestEqual(TEXT("错误门户地图不得进入插件开发"), FDBADevelopmentLaunchMode::Resolve(TEXT("-DBAPluginDemo -DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub_Fake"), false, false, false), Mode::Debug);
    TestEqual(TEXT("两项正式参数同时满足才能启动插件开发"), FDBADevelopmentLaunchMode::Resolve(TEXT("-DBAPluginDemo -DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub"), false, false, false), Mode::PluginDev);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDBADevelopmentLaunchSafetyTest,
    "DivineBeasts.PluginDev.LaunchMode.UnsafeHostsFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDBADevelopmentLaunchSafetyTest::RunTest(const FString&)
{
    using Mode = EDBADevelopmentLaunchMode;
    const TCHAR* Flag = TEXT("-DBAPluginDemo -DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub");
    TestEqual(TEXT("Shipping永远不进入开发门户"), FDBADevelopmentLaunchMode::Resolve(Flag, true, false, false), Mode::Debug);
    TestEqual(TEXT("Dedicated Server永远不进入开发门户"), FDBADevelopmentLaunchMode::Resolve(Flag, false, true, false), Mode::Debug);
    TestEqual(TEXT("Commandlet永远不进入开发门户"), FDBADevelopmentLaunchMode::Resolve(Flag, false, false, true), Mode::Debug);
    TestEqual(TEXT("FoundationStandalone互斥，不能同时装配"), FDBADevelopmentLaunchMode::Resolve(TEXT("-DBAPluginDemo -DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub -FoundationStandalone"), false, false, false), Mode::Debug);
    TestEqual(TEXT("FoundationOnlineIntegration互斥"), FDBADevelopmentLaunchMode::Resolve(TEXT("-DBAPluginDemo -DBAPluginDemoMap=/Game/Development/PluginDemos/Maps/L_DBA_PluginDemoHub -FoundationOnlineIntegration"), false, false, false), Mode::Debug);
    return true;
}
#endif

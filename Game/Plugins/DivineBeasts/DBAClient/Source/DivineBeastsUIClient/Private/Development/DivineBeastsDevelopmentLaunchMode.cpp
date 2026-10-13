// 开发启动模式解析不拥有UI、World、网络或Flow状态；仅按进程参数作一次安全分流。
#include "Development/DivineBeastsDevelopmentLaunchMode.h"
#include "Development/DivineBeastsPluginDemoMode.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

EDBADevelopmentLaunchMode FDBADevelopmentLaunchMode::Resolve(
    const TCHAR* CommandLine, bool bIsShipping, bool bDedicatedServer, bool bCommandlet)
{
    if (bIsShipping || bDedicatedServer || bCommandlet || !CommandLine)
    {
        return EDBADevelopmentLaunchMode::Debug;
    }
    // 兼容蓝图只读类型，但真实许可只委托现有的单一PluginDemo策略；
    // -DBALaunchMode=PluginDev从不构成授权，避免绕过登录却无法进入演示地图。
    FString RequestedMap;
    const bool bOptIn = FParse::Param(CommandLine, TEXT("DBAPluginDemo")) &&
        FParse::Value(CommandLine, TEXT("DBAPluginDemoMap="), RequestedMap);
    const bool bConflict = FParse::Param(CommandLine, TEXT("FoundationStandalone")) ||
        FParse::Param(CommandLine, TEXT("FoundationOnlineIntegration"));
    const bool bAllowed = FDivineBeastsPluginDemoMode::IsAllowedArguments(
        !bIsShipping, !bDedicatedServer && !bCommandlet,
        bOptIn, bConflict, RequestedMap);
    return bAllowed ? EDBADevelopmentLaunchMode::PluginDev : EDBADevelopmentLaunchMode::Debug;
}

EDBADevelopmentLaunchMode FDBADevelopmentLaunchMode::ResolveCurrent()
{
#if UE_BUILD_SHIPPING || UE_BUILD_TEST
    return EDBADevelopmentLaunchMode::Debug;
#else
    // 真正的运行时模式由ApplicationFlowClient中的唯一门禁定义，不重复解析启动参数。
    return FDivineBeastsPluginDemoMode::IsEnabled()
        ? EDBADevelopmentLaunchMode::PluginDev
        : EDBADevelopmentLaunchMode::Debug;
#endif
}

bool UDBADevelopmentLaunchModeLibrary::IsPluginDevelopmentMode()
{
    return FDBADevelopmentLaunchMode::ResolveCurrent() == EDBADevelopmentLaunchMode::PluginDev;
}

// 开发启动模式解析不拥有UI、World、网络或Flow状态；仅按进程参数作一次安全分流。
#include "Development/DivineBeastsDevelopmentLaunchMode.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

EDBADevelopmentLaunchMode FDBADevelopmentLaunchMode::Resolve(
    const TCHAR* CommandLine, bool bIsShipping, bool bDedicatedServer, bool bCommandlet)
{
    if (bIsShipping || bDedicatedServer || bCommandlet || !CommandLine)
    {
        return EDBADevelopmentLaunchMode::Debug;
    }
    FString Requested;
    if (!FParse::Value(CommandLine, TEXT("DBALaunchMode="), Requested) ||
        !Requested.Equals(TEXT("PluginDev"), ESearchCase::IgnoreCase))
    {
        // 未提供、非法或显式Debug都保持原生产启动；不会尝试自动登录。
        return EDBADevelopmentLaunchMode::Debug;
    }
    // 原Foundation验证流程会独占此实例ApplicationFlow，禁止与PluginDev争用同一世界/服务。
    if (FParse::Param(CommandLine, TEXT("FoundationStandalone")) ||
        FParse::Param(CommandLine, TEXT("FoundationOnlineIntegration")))
    {
        return EDBADevelopmentLaunchMode::Debug;
    }
    return EDBADevelopmentLaunchMode::PluginDev;
}

EDBADevelopmentLaunchMode FDBADevelopmentLaunchMode::ResolveCurrent()
{
#if UE_BUILD_SHIPPING
    return EDBADevelopmentLaunchMode::Debug;
#else
    return Resolve(FCommandLine::Get(), false, IsRunningDedicatedServer(), IsRunningCommandlet());
#endif
}

bool UDBADevelopmentLaunchModeLibrary::IsPluginDevelopmentMode()
{
    return FDBADevelopmentLaunchMode::ResolveCurrent() == EDBADevelopmentLaunchMode::PluginDev;
}

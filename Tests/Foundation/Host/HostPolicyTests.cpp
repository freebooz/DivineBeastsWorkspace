#include "Bootstrap/DBAFoundationPolicy.h"
#include <iostream>

int main()
{
    using namespace DBA::Foundation;
    // 捕获错误开启开发入口、Commandlet执行玩家流程和服务器创建玩家HUD三种回归。
    int Failed = 0;
    auto Check = [&Failed](bool Value, const char* Name) { if (!Value) { ++Failed; std::cerr << Name << '\n'; } };
    Check(ResolveMode(false, true, false, false) == EMode::Disabled, "ShippingCannotEnable");
    Check(ResolveMode(true, false, false, false) == EMode::Disabled, "ExplicitOptInRequired");
    Check(ResolveMode(true, true, true, false) == EMode::Disabled, "CommandletCannotRunPlayerFlow");
    Check(ResolveMode(true, true, false, true) == EMode::ServerDiagnostics, "DedicatedServerIsDiagnosticsOnly");
    Check(ResolveMode(true, true, false, false) == EMode::PlayerDevelopment, "ExplicitDevelopmentEnabled");
    std::cout << "Cases=5 Failed=" << Failed << '\n';
    return Failed ? 1 : 0;
}

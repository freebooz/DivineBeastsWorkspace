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
    Check(AcceptsWorldReady(true, true, "sandbox", "sandbox", "op-a", "op-a"), "MatchingWorldReady");
    Check(!AcceptsWorldReady(false, true, "sandbox", "sandbox", "op-a", "op-a"), "WrongInstanceRejected");
    Check(!AcceptsWorldReady(true, false, "sandbox", "sandbox", "op-a", "op-a"), "UnbegunWorldRejected");
    Check(!AcceptsWorldReady(true, true, "bootstrap", "sandbox", "op-a", "op-a"), "WrongMapRejected");
    Check(!AcceptsWorldReady(true, true, "sandbox", "sandbox", "op-old", "op-a"), "OldOperationRejected");
    Check(!AcceptsWorldReady(true, true, "sandbox", "sandbox", "", ""), "MissingOperationRejected");
    // 世界尚不存在也必须到期；修复若把截止检查放回世界就绪后，本策略不得放行。
    Check(StartupDeadlineExceeded(false, false, 61.0, 60.0), "MissingWorldStillTimesOut");
    Check(!StartupDeadlineExceeded(true, false, 61.0, 60.0), "StartedFlowUsesOwnDeadline");
    Check(!StartupDeadlineExceeded(false, true, 61.0, 60.0), "StoppedFlowDoesNotRestart");
    Check(!StartupDeadlineExceeded(false, false, 59.0, 60.0), "NotDueYet");
    Check(OwnsPendingTravel(true, "sandbox", "sandbox", "op-a", "op-a"), "OwnPendingTravelCanCancel");
    Check(!OwnsPendingTravel(true, "sandbox", "sandbox", "op-b", "op-a"), "OtherPendingTravelUntouched");
    Check(!OwnsPendingTravel(false, "sandbox", "sandbox", "op-a", "op-a"), "SuccessfulTravelNotCancelled");
    Check(!OwnsPendingTravel(true, "elsewhere", "sandbox", "op-a", "op-a"), "OtherDestinationUntouched");
    std::cout << "Cases=19 Failed=" << Failed << '\n';
    return Failed ? 1 : 0;
}

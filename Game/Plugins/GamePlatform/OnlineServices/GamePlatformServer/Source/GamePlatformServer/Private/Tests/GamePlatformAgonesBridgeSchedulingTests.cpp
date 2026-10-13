// Agones SDK 调度原生回归：只检查无网络副作用的优先级选择。
// 属于 GamePlatformServer（平台服务器插件）的服务器私有测试，不访问真实 Agones / Kubernetes。
#if WITH_DEV_AUTOMATION_TESTS

#include "Agones/GamePlatformAgonesBridgeSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformAgonesBridgeSchedulingTest,
    "GamePlatform.Server.Agones.BridgeScheduling",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformAgonesBridgeSchedulingTest::RunTest(const FString&)
{
    // 单独构造未 Initialize 的测试对象，禁止启动计时器或发送本地 HTTP 请求。
    // GameInstanceSubsystem 的 Outer 必须是 GameInstance；不要用 Package 冒充引擎宿主。
    // 只构造尚未初始化的宿主，不调用 Init/Initialize，不安装实际 HTTP/计时器。
    UGameInstance* TestInstance = NewObject<UGameInstance>(GetTransientPackage());
    if (!TestNotNull(TEXT("必须能够构造不联网的 GameInstance 宿主"), TestInstance))
    {
        return false;
    }
    UGamePlatformAgonesBridgeSubsystem* Bridge =
        NewObject<UGamePlatformAgonesBridgeSubsystem>(TestInstance);
    if (!TestNotNull(TEXT("必须能够构造不联网的调度对象"), Bridge))
    {
        return false;
    }

    using EOperation = UGamePlatformAgonesBridgeSubsystem::EAgonesSdkOperation;
    Bridge->NextHealthAtSeconds = 100.0;
    Bridge->NextRetryAtSeconds = 0.0;
    Bridge->bReadyRequested = true;

    // 即使健康检查恰好到期，Ready 也应获得一次调度机会；旧实现的 1 秒健康周期会使其饥饿。
    TestTrue(TEXT("健康到期时 Ready 不能长期饥饿"),
        Bridge->SelectNextSdkOperation(100.0) == EOperation::Ready);
    TestTrue(TEXT("超过两个调度周期时健康检查必须恢复优先级"),
        Bridge->SelectNextSdkOperation(102.1) == EOperation::Health);

    Bridge->bReadyAcknowledged = true;
    Bridge->bNeedsSelfAllocation = true;
    Bridge->LatestPlayers = 1;
    Bridge->LatestCapacity = 20;
    Bridge->CounterKey = TEXT("players");
    Bridge->bCounterDirty = true;
    TestTrue(TEXT("已 Ready 且真实有玩家时必须先保护共享实例"),
        Bridge->SelectNextSdkOperation(100.0) == EOperation::Allocate);

    Bridge->bSelfAllocated = true;
    TestTrue(TEXT("容量数据变化不会被每次到期的 Health 无限压制"),
        Bridge->SelectNextSdkOperation(100.0) == EOperation::Counter);

    Bridge->bCounterDirty = false;
    TestTrue(TEXT("没有待推进状态时按周期发送 Health"),
        Bridge->SelectNextSdkOperation(100.0) == EOperation::Health);
    Bridge->NextHealthAtSeconds = 110.0;
    TestTrue(TEXT("全部操作等待时不得发出多余 SDK 请求"),
        Bridge->SelectNextSdkOperation(100.0) == EOperation::None);

    Bridge->bShutdownRequested = true;
    TestTrue(TEXT("已申请关闭的服务器只允许最高优先级 Shutdown"),
        Bridge->SelectNextSdkOperation(100.0) == EOperation::Shutdown);
    Bridge->bShutdownAcknowledged = true;
    TestTrue(TEXT("SDK 确认关闭后不得重新 Ready 或 Health"),
        Bridge->SelectNextSdkOperation(100.0) == EOperation::None);

    Bridge->bShutdownRequested = false;
    Bridge->bShutdownAcknowledged = false;
    Bridge->bReadyAcknowledged = false;
    Bridge->bDraining = true;
    TestTrue(TEXT("排空中的服务器拒绝补发 Ready"),
        Bridge->SelectNextSdkOperation(100.0) == EOperation::None);
    return true;
}

#endif

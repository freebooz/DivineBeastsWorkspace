// 平台服务器心跳确认回归：模拟真实Provider终态回调，不访问网络或Agones。
// 失败、过期代次与退出不得伪造成功回执；有效回执必须保留确切人数和对应请求代次。
// 测试输入中0人仅模拟可信Provider收到的玩家计数，真实零玩家仍由Server World检验。
#if WITH_DEV_AUTOMATION_TESTS
#include "Server/GamePlatformServerLifecycleSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformServerHeartbeatAckFenceTest,
    "GamePlatform.Server.Control.HeartbeatAckGenerationFence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformServerHeartbeatAckFenceTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    TStrongObjectPtr<UGameInstance> Instance(NewObject<UGameInstance>());
    TStrongObjectPtr<UGamePlatformServerLifecycleSubsystem> Lifecycle(
        NewObject<UGamePlatformServerLifecycleSubsystem>(Instance.Get()));
    Lifecycle->Snapshot.State = EGamePlatformServerLifecycleState::Ready;

    Lifecycle->Generation = 5;
    Lifecycle->HeartbeatPlayersInFlight = 0;
    Lifecycle->bHeartbeatInFlight = true;
    Lifecycle->CompleteOperation(
        5, UGamePlatformServerLifecycleSubsystem::EControlOperation::Heartbeat,
        false, TEXT("ControlPlaneTransportFailed"));
    TestEqual(TEXT("传输失败不得承认0人心跳"), Lifecycle->GetLastAcknowledgedHeartbeatPlayers(), -1);
    TestEqual(TEXT("失败后的已确认代次必须仍为空"), Lifecycle->GetLastAcknowledgedHeartbeatGeneration(), uint64(0));

    Lifecycle->Generation = 6;
    Lifecycle->HeartbeatPlayersInFlight = 0;
    Lifecycle->bHeartbeatInFlight = true;
    Lifecycle->CompleteOperation(
        6, UGamePlatformServerLifecycleSubsystem::EControlOperation::Heartbeat, true, NAME_None);
    TestEqual(TEXT("真正成功的零心跳明确记录0人"), Lifecycle->GetLastAcknowledgedHeartbeatPlayers(), 0);
    TestEqual(TEXT("成功回执绑定当前请求代次"), Lifecycle->GetLastAcknowledgedHeartbeatGeneration(), uint64(6));

    Lifecycle->Generation = 7;
    Lifecycle->HeartbeatPlayersInFlight = 2;
    Lifecycle->bHeartbeatInFlight = true;
    Lifecycle->CompleteOperation(
        6, UGamePlatformServerLifecycleSubsystem::EControlOperation::Heartbeat, true, NAME_None);
    TestEqual(TEXT("旧请求完成不得覆盖已确认记录"), Lifecycle->GetLastAcknowledgedHeartbeatGeneration(), uint64(6));
    Lifecycle->CompleteOperation(
        7, UGamePlatformServerLifecycleSubsystem::EControlOperation::Heartbeat, true, NAME_None);
    TestEqual(TEXT("有玩家的成功心跳撤销可用于排空的零人数事实"),
              Lifecycle->GetLastAcknowledgedHeartbeatPlayers(), 2);
    TestEqual(TEXT("最后成功代次应随新心跳更新"),
              Lifecycle->GetLastAcknowledgedHeartbeatGeneration(), uint64(7));

    Lifecycle->Deinitialize();
    TestEqual(TEXT("退出后不得保留旧人数回执"), Lifecycle->GetLastAcknowledgedHeartbeatPlayers(), -1);
    TestEqual(TEXT("退出后不得保留旧操作代次"), Lifecycle->GetLastAcknowledgedHeartbeatGeneration(), uint64(0));
    return true;
}
#endif

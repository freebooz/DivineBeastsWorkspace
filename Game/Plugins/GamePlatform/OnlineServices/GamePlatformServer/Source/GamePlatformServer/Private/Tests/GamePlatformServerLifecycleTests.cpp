#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Server/GamePlatformServerLifecycleSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformServerInstanceInfoValidationTest,
    "GamePlatform.Server.InstanceInfo.Validation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGamePlatformServerInstanceInfoValidationTest::RunTest(const FString&)
{
    FGamePlatformServerInstanceInfo Instance;
    Instance.GameId = TEXT("divine-beasts");
    Instance.GameServerId = TEXT("server-001");
    Instance.ServerRoleId = TEXT("GameServer.Role.MainArena");
    Instance.ExperienceId = TEXT("Experience.MainArena.Main");
    Instance.WorldId = TEXT("World.MainArena.Main");
    Instance.RegionId = TEXT("test-region");
    Instance.ClusterId = TEXT("cluster-a");
    Instance.NodeId = TEXT("node-01");
    Instance.BuildVersion = TEXT("0.1.0-dev");
    Instance.ProtocolVersion = 2;
    Instance.PublicEndpoint = TEXT("127.0.0.1:7777");
    Instance.Capacity = 10;

    TestTrue(TEXT("完整非秘密注册信息有效"), Instance.IsValid());

    Instance.Capacity = 0;
    TestFalse(TEXT("非正容量必须拒绝"), Instance.IsValid());
    Instance.Capacity = 10;

    Instance.ProtocolVersion = -1;
    TestFalse(TEXT("负网络协议版本必须拒绝"), Instance.IsValid());
    Instance.ProtocolVersion = 2;

    Instance.ClusterId = TEXT("cluster-a\nX-Injected: true");
    TestFalse(TEXT("可选集群字段同样拒绝换行注入"), Instance.IsValid());
    Instance.ClusterId = TEXT("cluster-a");

    Instance.PublicEndpoint += TEXT("\nAuthorization: secret");
    TestFalse(TEXT("端点换行必须拒绝以防止头注入"), Instance.IsValid());
    return true;
}

#endif

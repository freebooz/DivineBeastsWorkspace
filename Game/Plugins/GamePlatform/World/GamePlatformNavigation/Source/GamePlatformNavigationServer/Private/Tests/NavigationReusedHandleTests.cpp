// 引擎服务行为回归：无需NavMesh的记录夹具驱动真实完成/取消路径，不冒充真实导航计算或联机。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformNavigationWorldSubsystem.h"
#include "Engine/World.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavigationReusedHandleTest,
    "GamePlatform.Navigation.Server.OldHandleCannotCancelReusedRequest",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FNavigationReusedHandleTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // UE5.8的CreateWorld已初始化世界；参数一次性传入，避免二次创建固定名WorldSettings而崩溃。
    const UWorld::InitializationValues WorldInitializationValues = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false)
        .CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &WorldInitializationValues);
    auto* Service = World->GetSubsystem<UGamePlatformNavigationWorldSubsystem>(); const FGuid RequestId = FGuid::NewGuid();
    int32 CompletedA = 0; int32 CompletedB = 0;
    const auto AddRecord = [&](uint64 Generation, uint32 QueryId, int32& CompletionCount)
    {
        auto& Record = Service->AsyncRequests.Add(RequestId); Record.OperationGeneration = Generation;
        Record.EngineQueryId = QueryId; Record.WorldGeneration = Service->WorldGeneration;
        Record.Request.RequestId = RequestId; Record.Request.WorldGeneration = Service->WorldGeneration;
        Record.Completion = FGamePlatformNavigationQueryCompleted::CreateLambda([&CompletionCount](const auto&) { ++CompletionCount; });
    };
    AddRecord(1, 11, CompletedA); FGamePlatformNavigationRequestHandle HandleA;
    HandleA.RequestId = RequestId; HandleA.WorldGeneration = Service->WorldGeneration; HandleA.OperationGeneration = 1;
    Service->HandleAsyncPathCompleted(11, ENavigationQueryResult::Error, {}, RequestId, Service->WorldGeneration, 1, {}, {});
    TestEqual(TEXT("A产生自己的终态回调"), CompletedA, 1); AddRecord(2, 12, CompletedB);
    TestFalse(TEXT("旧A句柄取消明确拒绝"), Service->CancelRequest(HandleA));
    TestTrue(TEXT("B的记录仍存在"), Service->AsyncRequests.Contains(RequestId));
    Service->HandleAsyncPathCompleted(12, ENavigationQueryResult::Error, {}, RequestId, Service->WorldGeneration, 2, {}, {});
    TestEqual(TEXT("B仍独立完成且只完成一次"), CompletedB, 1); TestFalse(TEXT("B完成后释放记录"), Service->AsyncRequests.Contains(RequestId));
    World->DestroyWorld(false); return true;
}
#endif

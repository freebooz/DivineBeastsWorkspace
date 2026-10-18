// 引擎服务行为回归：验证完成/取消所有权及无缓存过滤器初始化；夹具无烘焙导航瓦片，不冒充真实寻路或联机。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Subsystems/GamePlatformNavigationWorldSubsystem.h"
#include "Engine/World.h"
#include "Filters/GamePlatformNavigationQueryFilters.h"
#include "NavigationSystem.h"
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FNavigationColdFilterTest,
    "GamePlatform.Navigation.Server.ColdFilterInitializesBeforeQuery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ServerContext | EAutomationTestFlags::EngineFilter)
bool FNavigationColdFilterTest::RunTest(const FString& Parameters)
{
    (void)Parameters;
    // 真实World/NavData与平台公共查询入口，刻意不构建瓦片：期望到达原生查询并报无路径，不能在过滤器阶段误拒绝。
    const UWorld::InitializationValues Initialization = UWorld::InitializationValues().AllowAudioPlayback(false)
        .CreatePhysicsScene(false).CreateNavigation(true).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true,
        ERHIFeatureLevel::Num, &Initialization);
    if (!TestNotNull(TEXT("创建独立导航测试World"), World)) return false;
    auto* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
    auto* Service = World->GetSubsystem<UGamePlatformNavigationWorldSubsystem>();
    if (!TestNotNull(TEXT("真实导航系统存在"), Navigation) || !TestNotNull(TEXT("平台导航服务存在"), Service))
    {
        World->DestroyWorld(false);
        return false;
    }
    ANavigationData* NavData = Navigation->GetDefaultNavDataInstance(FNavigationSystem::Create);
    if (!TestNotNull(TEXT("创建引擎默认NavData"), NavData))
    {
        World->DestroyWorld(false);
        return false;
    }

    const TSubclassOf<UNavigationQueryFilter> FilterClass = UGamePlatformNavigationQueryFilter_Default::StaticClass();
    NavData->RemoveQueryFilter(FilterClass);
    TestFalse(TEXT("首次平台查询前自定义默认过滤器缓存为空"), NavData->GetQueryFilter(FilterClass).IsValid());
    FVector Location(123.0);
    EGamePlatformNavigationError Error = EGamePlatformNavigationError::None;
    TestFalse(TEXT("无瓦片世界不能伪造随机可达点"),
        Service->FindRandomReachablePoint(FVector(0, 0, 100), 500.0f, NAME_None, NAME_None, Location, Error));
    TestEqual(TEXT("冷查询通过过滤器阶段，原生无瓦片返回PathNotFound"), Error, EGamePlatformNavigationError::PathNotFound);
    TestTrue(TEXT("失败输出位置清零"), Location.IsZero());
    const FSharedConstNavQueryFilter FirstFilter = NavData->GetQueryFilter(FilterClass);
    TestTrue(TEXT("首次查询已创建并缓存平台过滤器"), FirstFilter.IsValid());

    // 默认ID和空ID同义，重复查询复用该NavData的引擎缓存；显式未知ID仍必须失败，不能暗中换成默认配置。
    Service->FindRandomReachablePoint(FVector::ZeroVector, 500.0f, TEXT("Navigation.Agent.Default"),
        TEXT("Navigation.Filter.Default"), Location, Error);
    TestEqual(TEXT("显式默认配置同样到达原生查询"), Error, EGamePlatformNavigationError::PathNotFound);
    TestTrue(TEXT("重复查询复用当前NavData的过滤器缓存"), FirstFilter == NavData->GetQueryFilter(FilterClass));
    Service->FindRandomReachablePoint(FVector::ZeroVector, 500.0f, NAME_None, TEXT("Navigation.Filter.Unknown"), Location, Error);
    TestEqual(TEXT("未知过滤器保持明确拒绝"), Error, EGamePlatformNavigationError::InvalidFilter);
    Service->FindRandomReachablePoint(FVector::ZeroVector, 500.0f, TEXT("Navigation.Agent.Unknown"), NAME_None, Location, Error);
    TestEqual(TEXT("未知代理保持明确拒绝"), Error, EGamePlatformNavigationError::InvalidAgentProfile);

    // 投影也共享ResolveQueryContext；清除缓存后再次验证，防止只在随机查询调用方偶然预热掩盖根因。
    NavData->RemoveQueryFilter(FilterClass);
    const FGamePlatformNavigationProjectionResult Projection = Service->ProjectPoint(
        FVector(0, 0, 100), FVector(130, 130, 550), NAME_None, NAME_None);
    TestFalse(TEXT("无瓦片投影不能伪报成功"), Projection.bSuccess);
    TestEqual(TEXT("冷投影完成过滤器初始化后返回ProjectionFailed"), Projection.Error, EGamePlatformNavigationError::ProjectionFailed);
    TestTrue(TEXT("冷投影同样创建当前NavData的过滤器缓存"), NavData->GetQueryFilter(FilterClass).IsValid());
    World->DestroyWorld(false);
    return true;
}
#endif

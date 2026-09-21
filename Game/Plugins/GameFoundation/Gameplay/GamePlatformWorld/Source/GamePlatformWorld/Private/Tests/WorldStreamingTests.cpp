// UE自动化测试：先于流送实现编写。本轮禁止启动UE，以下用例尚未编译/执行。
// 临时World和已登记的空关卡只验证协调行为，不伪装真实地图加载或WP完成。
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Streaming/GamePlatformWorldStreaming.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/World.h"
#include <limits>

namespace
{
    struct FStreamingTestWorld
    {
        UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
        ~FStreamingTestWorld() { if (World) { World->DestroyWorld(false); } }

        ULevelStreamingDynamic* AddLevel(FName Package)
        {
            ULevelStreamingDynamic* Level = NewObject<ULevelStreamingDynamic>(World);
            Level->SetWorldAssetByPackageName(Package);
            Level->SetShouldBeLoaded(false);
            Level->SetShouldBeVisible(false);
            World->AddStreamingLevel(Level);
            return Level;
        }

        FGamePlatformWorldStreamingRequest Request(FGuid Generation) const
        {
            FGamePlatformWorldStreamingRequest Result;
            Result.ContextGeneration = Generation;
            Result.Owner = World;
            Result.LevelPackage = TEXT("/Game/WorldStreamingTests/ExistingLevel");
            Result.bRequiredForReadiness = true;
            return Result;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldStreamingValidationTest,
    "GamePlatform.World.Streaming.RejectInvalidWithoutChangingLevels",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldStreamingValidationTest::RunTest(const FString& Parameters)
{
    FStreamingTestWorld Fixture;
    if (!TestNotNull(TEXT("临时世界创建"), Fixture.World)) { return false; }
    const FGuid Generation = FGuid::NewGuid();
    FGamePlatformWorldStreaming Streaming(*Fixture.World, Generation);
    ULevelStreamingDynamic* Level = Fixture.AddLevel(TEXT("/Game/WorldStreamingTests/ExistingLevel"));
    const auto Valid = Fixture.Request(Generation);
    auto Reject = [&](FGamePlatformWorldStreamingRequest Request, FName Code)
    {
        FGamePlatformResult Result;
        TestFalse(TEXT("拒绝时无有效句柄"), Streaming.Request(Request, Result).IsValid());
        TestEqual(TEXT("精确错误分类"), Result.Code, Code);
        TestFalse(TEXT("校验失败不修改关卡加载标志"), Level->ShouldBeLoaded());
    };
    auto Request = Valid;
    Request.ContextGeneration = FGuid::NewGuid();
    Reject(Request, TEXT("WorldStreamingGenerationMismatch"));
    Request = Valid; Request.Owner.Reset();
    Reject(Request, TEXT("WorldStreamingOwnerInvalid"));
    FStreamingTestWorld Foreign;
    Request = Valid; Request.Owner = Foreign.World;
    Reject(Request, TEXT("WorldStreamingOwnerInvalid"));
    Request = Valid; Request.TimeoutSeconds = 0;
    Reject(Request, TEXT("WorldStreamingInvalidRequest"));
    Request = Valid; Request.TimeoutSeconds = std::numeric_limits<double>::infinity();
    Reject(Request, TEXT("WorldStreamingInvalidRequest"));
    Request = Valid; Request.TargetLocation.X = std::numeric_limits<double>::quiet_NaN();
    Reject(Request, TEXT("WorldStreamingInvalidRequest"));
    Request = Valid; Request.Priority = -1;
    Reject(Request, TEXT("WorldStreamingInvalidRequest"));
    Request = Valid; Request.Priority = 256;
    Reject(Request, TEXT("WorldStreamingInvalidRequest"));
    Request = Valid; Request.RequestedState = static_cast<EGamePlatformWorldStreamingRequestedState>(255);
    Reject(Request, TEXT("WorldStreamingInvalidRequest"));
    Request = Valid; Request.LevelPackage = NAME_None;
    Reject(Request, TEXT("WorldStreamingInvalidRequest"));
    Request = Valid; Request.LevelPackage = TEXT("/Game/WorldStreamingTests/MissingLevel");
    Reject(Request, TEXT("WorldStreamingLevelNotFound"));
    TestEqual(TEXT("没有创建额外加载器"), Fixture.World->GetStreamingLevels().Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldStreamingCancellationTest,
    "GamePlatform.World.Streaming.CancelDoesNotUnloadSharedLevel",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldStreamingCancellationTest::RunTest(const FString& Parameters)
{
    FStreamingTestWorld Fixture;
    if (!TestNotNull(TEXT("临时世界创建"), Fixture.World)) { return false; }
    const FGuid Generation = FGuid::NewGuid();
    FGamePlatformWorldStreaming Streaming(*Fixture.World, Generation);
    ULevelStreamingDynamic* Level = Fixture.AddLevel(TEXT("/Game/WorldStreamingTests/ExistingLevel"));
    auto Request = Fixture.Request(Generation);
    Request.RequestedState = EGamePlatformWorldStreamingRequestedState::Activated;
    FGamePlatformResult Result;
    const auto First = Streaming.Request(Request, Result);
    TestTrue(TEXT("接受合法请求"), First.IsValid() && Result.IsSuccess());
    const auto Second = Streaming.Request(Request, Result);
    TestTrue(TEXT("同关卡独立需求"), Second.IsValid() && Second.RequestId != First.RequestId);
    TestTrue(TEXT("已要求加载"), Level->ShouldBeLoaded());
    TestTrue(TEXT("已要求可见"), Level->ShouldBeVisible());
    Streaming.Tick();
    TestTrue(TEXT("提交不冒充完成"), Streaming.GetState(First).State == EGamePlatformWorldStreamingState::Pending);
    TestFalse(TEXT("必需请求仍等待"), Streaming.IsRequiredReady());
    TestTrue(TEXT("取消成功"), Streaming.Cancel(First).IsSuccess());
    TestTrue(TEXT("精确句柄重复取消幂等"), Streaming.Cancel(First).IsSuccess());
    TestTrue(TEXT("取消后不会迟到成功"), Streaming.GetState(First).State == EGamePlatformWorldStreamingState::Cancelled);
    TestTrue(TEXT("其他需求仍Pending"), Streaming.GetState(Second).State == EGamePlatformWorldStreamingState::Pending);
    TestTrue(TEXT("不卸载共享关卡"), Level->ShouldBeLoaded() && Level->ShouldBeVisible());
    auto ForeignHandle = Second; ForeignHandle.ContextGeneration = FGuid::NewGuid();
    TestFalse(TEXT("旧世界句柄不能取消新请求"), Streaming.Cancel(ForeignHandle).IsSuccess());
    Streaming.Cancel(Second);
    TestTrue(TEXT("显式取消撤销本请求就绪义务"), Streaming.IsRequiredReady());
    TestFalse(TEXT("显式取消不是遗留失败"), Streaming.HasRequiredFailure());
    Streaming.Shutdown(); Streaming.Shutdown(); Streaming.Tick();
    TestFalse(TEXT("关闭不可Ready"), Streaming.IsRequiredReady());
    TestFalse(TEXT("关闭拒绝新请求"), Streaming.Request(Request, Result).IsValid());
    TestEqual(TEXT("关闭诊断"), Result.Code, FName(TEXT("WorldStreamingClosed")));
    TestTrue(TEXT("关闭也不卸载他人关卡"), Level->ShouldBeLoaded() && Level->ShouldBeVisible());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldStreamingFailureTest,
    "GamePlatform.World.Streaming.OwnerLossTimeoutAndExternalRemovalFailClosed",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldStreamingFailureTest::RunTest(const FString& Parameters)
{
    FStreamingTestWorld Fixture;
    if (!TestNotNull(TEXT("临时世界创建"), Fixture.World)) { return false; }
    const FGuid Generation = FGuid::NewGuid();
    FGamePlatformWorldStreaming Streaming(*Fixture.World, Generation);
    auto* Level = Fixture.AddLevel(TEXT("/Game/WorldStreamingTests/ExistingLevel"));
    auto Request = Fixture.Request(Generation);
    UObject* Owner = NewObject<UObject>(Fixture.World);
    Request.Owner = Owner;
    FGamePlatformResult Result;
    const auto Owned = Streaming.Request(Request, Result);
    TestTrue(TEXT("世界Outer拥有者可用"), Owned.IsValid());
    Owner->MarkAsGarbage();
    Streaming.Tick();
    TestEqual(TEXT("拥有者失效失败码"), Streaming.GetState(Owned).Error, FName(TEXT("WorldStreamingOwnerInvalid")));
    TestTrue(TEXT("失效需求不能悄悄移出必需屏障"), Streaming.HasRequiredFailure());
    TestFalse(TEXT("失败不能Ready"), Streaming.IsRequiredReady());
    Streaming.Cancel(Owned);

    Request = Fixture.Request(Generation);
    Request.TimeoutSeconds = std::numeric_limits<double>::min();
    const auto Timed = Streaming.Request(Request, Result);
    Streaming.Tick();
    TestEqual(TEXT("未加载关卡真实超时"), Streaming.GetState(Timed).Error, FName(TEXT("WorldStreamingTimeout")));
    TestTrue(TEXT("超时记入必需失败"), Streaming.HasRequiredFailure());
    Streaming.Cancel(Timed);

    Request = Fixture.Request(Generation);
    const auto Removed = Streaming.Request(Request, Result);
    Fixture.World->RemoveStreamingLevel(Level);
    Streaming.Tick();
    TestEqual(TEXT("外部移除不能沿用旧对象"), Streaming.GetState(Removed).Error, FName(TEXT("WorldStreamingLevelRemoved")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldStreamingExternalChangeTest,
    "GamePlatform.World.Streaming.ExternalChangesAreNotRestored",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldStreamingExternalChangeTest::RunTest(const FString& Parameters)
{
    FStreamingTestWorld Fixture;
    if (!TestNotNull(TEXT("临时世界创建"), Fixture.World)) { return false; }
    const FGuid Generation = FGuid::NewGuid();
    FGamePlatformWorldStreaming Streaming(*Fixture.World, Generation);
    auto* Level = Fixture.AddLevel(TEXT("/Game/WorldStreamingTests/ExistingLevel"));
    auto Request = Fixture.Request(Generation);
    Request.RequestedState = EGamePlatformWorldStreamingRequestedState::Activated;
    FGamePlatformResult Result;
    const auto Handle = Streaming.Request(Request, Result);
    Level->SetShouldBeVisible(false);
    Level->SetPriority(900);
    Streaming.Tick();
    TestEqual(TEXT("外部撤回需求明确失败"), Streaming.GetState(Handle).Error, FName(TEXT("WorldStreamingExternalChange")));
    Streaming.Cancel(Handle);
    TestFalse(TEXT("取消不回写外部可见性"), Level->ShouldBeVisible());
    TestEqual(TEXT("取消不恢复旧优先级"), Level->GetPriority(), 900);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldStreamingAmbiguousPackageTest,
    "GamePlatform.World.Streaming.AmbiguousPackageDoesNotPickFirst",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldStreamingAmbiguousPackageTest::RunTest(const FString& Parameters)
{
    FStreamingTestWorld Fixture;
    if (!TestNotNull(TEXT("临时世界创建"), Fixture.World)) { return false; }
    const FGuid Generation = FGuid::NewGuid();
    FGamePlatformWorldStreaming Streaming(*Fixture.World, Generation);
    auto* First = Fixture.AddLevel(TEXT("/Game/WorldStreamingTests/ExistingLevel"));
    auto* Second = Fixture.AddLevel(TEXT("/Game/WorldStreamingTests/ExistingLevel"));
    FGamePlatformResult Result;
    TestFalse(TEXT("重复已登记包拒绝"), Streaming.Request(Fixture.Request(Generation), Result).IsValid());
    TestEqual(TEXT("歧义诊断"), Result.Code, FName(TEXT("WorldStreamingAmbiguousLevel")));
    TestFalse(TEXT("不修改任一候选"), First->ShouldBeLoaded() || Second->ShouldBeLoaded());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWorldStreamingTearDownTest,
    "GamePlatform.World.Streaming.BeginTearDownCancelsBeforeEndPlay",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FWorldStreamingTearDownTest::RunTest(const FString& Parameters)
{
    FStreamingTestWorld Fixture;
    if (!TestNotNull(TEXT("临时世界创建"), Fixture.World)) { return false; }
    const FGuid Generation = FGuid::NewGuid();
    FGamePlatformWorldStreaming Streaming(*Fixture.World, Generation);
    Fixture.AddLevel(TEXT("/Game/WorldStreamingTests/ExistingLevel"));
    FGamePlatformResult Result;
    const auto Handle = Streaming.Request(Fixture.Request(Generation), Result);
    Fixture.World->BeginTearingDown();
    TestTrue(TEXT("没有BeginPlay也立即撤销"), Streaming.GetState(Handle).State == EGamePlatformWorldStreamingState::Cancelled);
    TestFalse(TEXT("无需后续Tick就关闭就绪屏障"), Streaming.IsRequiredReady());
    TestFalse(TEXT("关闭拒绝新请求"), Streaming.Request(Fixture.Request(Generation), Result).IsValid());
    return true;
}

IMPLEM
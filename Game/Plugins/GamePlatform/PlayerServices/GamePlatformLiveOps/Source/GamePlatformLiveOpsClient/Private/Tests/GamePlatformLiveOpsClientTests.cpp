// 平台运营客户端事件回归；测试Port仅手控快照/时序，真实Ticker验证时间边界，禁止用于生产或授奖。
#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/GamePlatformLiveOpsClientTransport.h"
#include "Misc/AutomationTest.h"
#include "Services/GamePlatformLiveOpsClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Subsystems/SubsystemCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "HAL/PlatformTime.h"

namespace
{
/**
 * 仅Automation的合法Outer夹具：LocalPlayer的Within是Engine，领域Subsystem的Within是LocalPlayer。
 * GT显式构造并强持有两者，不PlayerAdded/不建World或自动登录；原测试Transport/账号前提保持。
 * 无Viewport时GetGameInstance为nullptr，不能把本夹具当完整GI/Online装配或生产服务。
 * 任意正常/提前返回先Deinitialize清委托/取消请求，再释放Client与Player，避免GC和测试间残留。
 */
struct FLiveOpsLocalPlayerFixture
{
    TStrongObjectPtr<ULocalPlayer> Player;
    TStrongObjectPtr<UGamePlatformLiveOpsClientSubsystem> Client;
    bool Initialize(FAutomationTestBase& Test)
    {
        if (!Test.TestNotNull(TEXT("LocalPlayer真实Engine Within宿主"), GEngine)) return false;
        Player.Reset(NewObject<ULocalPlayer>(GEngine));
        if (!Test.TestNotNull(TEXT("领域Subsystem真实LocalPlayer Outer"), Player.Get())) return false;
        Client.Reset(NewObject<UGamePlatformLiveOpsClientSubsystem>(Player.Get()));
        return Test.TestNotNull(TEXT("合法Outer的领域Subsystem实例"), Client.Get());
    }
    ~FLiveOpsLocalPlayerFixture()
    {
        if (Client.IsValid()) Client->Deinitialize();
        Client.Reset();
        Player.Reset();
    }
};

class FLiveOpsMockTransport final
    : public IGamePlatformLiveOpsClientTransport
{
public:
    FGamePlatformLiveOpsCatalogCompletion CatalogCompletion;
    FGamePlatformLiveOpsPlayerStateCompletion StateCompletion;
    FGamePlatformLiveOpsClaimCompletion ClaimCompletion;

    virtual void CancelAllRequests() override
    {
        CatalogCompletion = {};
        StateCompletion = {};
        ClaimCompletion = {};
    }

    virtual bool BeginGetCatalog(
        FGamePlatformLiveOpsCatalogCompletion Completion) override
    {
        CatalogCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginGetPlayerState(
        FGamePlatformLiveOpsPlayerStateCompletion Completion) override
    {
        StateCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginClaimSignIn(
        const FGuid&,
        FName,
        FGamePlatformLiveOpsClaimCompletion Completion) override
    {
        ClaimCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginQueryClaimOperation(
        const FGuid&,
        FGamePlatformLiveOpsClaimCompletion Completion) override
    {
        ClaimCompletion = MoveTemp(Completion);
        return true;
    }
};

FGamePlatformLiveOpsCatalogSnapshot CatalogSnapshot(int64 Revision)
{
    FGamePlatformLiveOpsCatalogSnapshot Value;
    Value.CatalogVersion = 1;
    Value.CatalogRevision = Revision;
    Value.ServerTimeUtc = FDateTime(2026, 9, 24, 12, 0, 0);
    return Value;
}

FGamePlatformLiveOpsPlayerState PlayerState(int64 Revision)
{
    FGamePlatformLiveOpsPlayerState Value;
    Value.PlayerStateRevision = Revision;
    Value.GeneratedAtUtc = FDateTime(2026, 9, 24, 12, 0, 0);
    Value.ServerTimeUtc = Value.GeneratedAtUtc;
    return Value;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformLiveOpsAccountIsolationTest,
    "GamePlatform.LiveOps.Client.AccountIsolation",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformLiveOpsAccountIsolationTest::RunTest(const FString&)
{
    FLiveOpsLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformLiveOpsClientSubsystem* Client = Fixture.Client.Get();

    TSharedPtr<FLiveOpsMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FLiveOpsMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("配置账号启动Catalog/PlayerState请求"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-A"),
            Transport));

    FGamePlatformLiveOpsCatalogCompletion OldCatalog =
        MoveTemp(Transport->CatalogCompletion);
    FGamePlatformLiveOpsPlayerStateCompletion OldState =
        MoveTemp(Transport->StateCompletion);

    Client->ResetAccount();

    OldCatalog(
        CatalogSnapshot(9),
        EGamePlatformLiveOpsError::None);
    OldState(
        PlayerState(9),
        EGamePlatformLiveOpsError::None);

    TestEqual(
        TEXT("旧账号Catalog响应不能污染已重置客户端"),
        Client->GetCatalogRevision(),
        int64(0));

    TestEqual(
        TEXT("旧账号PlayerState响应不能污染已重置客户端"),
        Client->GetPlayerStateRevision(),
        int64(0));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformLiveOpsRevisionTest,
    "GamePlatform.LiveOps.Client.Revision",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformLiveOpsRevisionTest::RunTest(const FString&)
{
    FLiveOpsLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformLiveOpsClientSubsystem* Client = Fixture.Client.Get();

    TSharedPtr<FLiveOpsMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FLiveOpsMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("首次请求已启动"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-A"),
            Transport));

    Transport->CatalogCompletion(
        CatalogSnapshot(2),
        EGamePlatformLiveOpsError::None);
    Transport->StateCompletion(
        PlayerState(2),
        EGamePlatformLiveOpsError::None);

    TestEqual(
        TEXT("Catalog revision应用"),
        Client->GetCatalogRevision(),
        int64(2));

    TestTrue(
        TEXT("刷新Catalog启动"),
        Client->RefreshCatalog());

    Transport->CatalogCompletion(
        CatalogSnapshot(1),
        EGamePlatformLiveOpsError::None);

    TestEqual(
        TEXT("旧Catalog revision被忽略"),
        Client->GetCatalogRevision(),
        int64(2));

    return true;
}

// 相同服务器Revision也可伴随时间采样推进；UI必须收到派生视图刷新，不能自行轮询。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsSameRevisionViewTest, "GamePlatform.LiveOps.Client.SameRevisionView", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLiveOpsSameRevisionViewTest::RunTest(const FString&)
{
    FLiveOpsLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformLiveOpsClientSubsystem* Client = Fixture.Client.Get();
    auto Transport = MakeShared<FLiveOpsMockTransport, ESPMode::ThreadSafe>();
    Client->ConfigureAuthenticatedAccount(TEXT("Boundary"), Transport);
    Transport->CatalogCompletion(CatalogSnapshot(1), EGamePlatformLiveOpsError::None);
    Transport->StateCompletion(PlayerState(1), EGamePlatformLiveOpsError::None);
    int32 CatalogEvents = 0; int32 PlayerEvents = 0;
    Client->OnCatalogChanged.AddLambda([&]() { ++CatalogEvents; });
    Client->OnPlayerStateChanged.AddLambda([&]() { ++PlayerEvents; });
    Client->RefreshAll();
    auto Catalog = CatalogSnapshot(1); Catalog.ServerTimeUtc += FTimespan::FromHours(1);
    auto State = PlayerState(1); State.ServerTimeUtc += FTimespan::FromHours(1);
    Transport->CatalogCompletion(Catalog, EGamePlatformLiveOpsError::None);
    Transport->StateCompletion(State, EGamePlatformLiveOpsError::None);
    TestTrue(TEXT("同Revision目录时间刷新通知"), CatalogEvents > 0);
    TestTrue(TEXT("同Revision签到状态刷新通知"), PlayerEvents > 0);
    const int32 Previous = PlayerEvents; Client->ResetAccount();
    TestTrue(TEXT("清空账号通知玩家视图"), PlayerEvents > Previous);
    Client->OnCatalogChanged.Clear(); Client->OnPlayerStateChanged.Clear();
    return true;
}
// 真实核心Ticker跨越活动开始/结束；网络刷新不完成时，既有Revision仍须发布派生视图通知。
namespace
{
class FLiveOpsBoundaryCommand final : public IAutomationLatentCommand
{
public:
    explicit FLiveOpsBoundaryCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FLiveOpsBoundaryCommand() override { if (Client.Get()) { Client->Deinitialize(); } }
    bool Update() override
    {
        if (FPlatformTime::Seconds() - Started > 15.0) { Test->AddError(TEXT("活动真实Ticker边界通知超时")); return true; }
        if (!Client.Get())
        {
            // 潜伏命令跨帧持有合法Player Outer；缺Engine记录真实失败并结束，不反复创建非法对象。
            if (!GEngine) { Test->AddError(TEXT("LiveOps跨帧夹具缺真实Engine Within宿主")); return true; }
            Player.Reset(NewObject<ULocalPlayer>(GEngine));
            Client.Reset(NewObject<UGamePlatformLiveOpsClientSubsystem>(Player.Get()));
            Client->Initialize(Collection);
            Transport = MakeShared<FLiveOpsMockTransport, ESPMode::ThreadSafe>();
            Client->ConfigureAuthenticatedAccount(TEXT("BoundaryTicker"), Transport);
            auto Catalog = CatalogSnapshot(1);
            FGamePlatformLiveOpsSeason Season; Season.SeasonId = TEXT("Window");
            Season.TimeWindow.StartsAtUtc = Catalog.ServerTimeUtc + FTimespan::FromSeconds(0.5);
            Season.TimeWindow.bHasEnd = true; Season.TimeWindow.EndsAtUtc = Catalog.ServerTimeUtc + FTimespan::FromSeconds(1.0);
            Catalog.Seasons.Add(Season);
            auto CatalogComplete = MoveTemp(Transport->CatalogCompletion); CatalogComplete(Catalog, EGamePlatformLiveOpsError::None);
            auto PlayerComplete = MoveTemp(Transport->StateCompletion); PlayerComplete(PlayerState(1), EGamePlatformLiveOpsError::None);
            Client->OnCatalogChanged.AddLambda([this]() { ++CatalogEvents; });
            Client->OnPlayerStateChanged.AddLambda([this]() { ++PlayerEvents; });
            // 超时/退出先Deinitialize解绑这些委托，Ticker不保留潜伏命令地址。
            return false;
        }
        if (FPlatformTime::Seconds() - Started < 2.0 || CatalogEvents == 0 || PlayerEvents == 0) { return false; }
        Test->TestEqual(TEXT("派生边界通知无需目录Revision提高"), Client->GetCatalogRevision(), int64(1));
        Test->TestTrue(TEXT("跨结束边界后活动不再有效"), Client->GetActiveSeasonIds().IsEmpty());
        Test->TestTrue(TEXT("边界刷新请求已受理但未假造后端结果"), !!Transport->CatalogCompletion);
        return true;
    }
private:
    FAutomationTestBase* Test;
    double Started = FPlatformTime::Seconds();
    int32 CatalogEvents = 0, PlayerEvents = 0;
    FSubsystemCollection<ULocalPlayerSubsystem> Collection;
    // 在Client之前声明，析构反序保证Client退出/释放时其真实LocalPlayer Outer仍被强持有。
    TStrongObjectPtr<ULocalPlayer> Player;
    TStrongObjectPtr<UGamePlatformLiveOpsClientSubsystem> Client;
    TSharedPtr<FLiveOpsMockTransport, ESPMode::ThreadSafe> Transport;
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsBoundaryNotificationTest, "GamePlatform.LiveOps.Client.TimeBoundary", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLiveOpsBoundaryNotificationTest::RunTest(const FString&)
{ ADD_LATENT_AUTOMATION_COMMAND(FLiveOpsBoundaryCommand(this)); return true; }

// 生命周期回归：测试Transport不访问网络；Reset同步通知调用Deinitialize后，关闭作用域必须拒绝恢复账号及公开刷新。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsCloseDuringConfigureTest, "GamePlatform.LiveOps.Client.CloseDuringConfigure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLiveOpsCloseDuringConfigureTest::RunTest(const FString&)
{
    FLiveOpsLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformLiveOpsClientSubsystem* Client = Fixture.Client.Get();
    auto Transport = MakeShared<FLiveOpsMockTransport, ESPMode::ThreadSafe>();
    TestTrue(TEXT("前置账号请求成功受理"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Previous"), Transport));
    Client->OnViewChanged.AddLambda([Client](auto&&...) { Client->Deinitialize(); });
    TestFalse(TEXT("Reset通知内关闭后Configure不得复活服务"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Closed"), Transport));
    TestFalse(TEXT("关闭后不得启动请求"), static_cast<bool>(Transport->CatalogCompletion));
    TestFalse(TEXT("公开刷新拒绝已关闭作用域"), Client->RefreshCatalog());
    return true;
}


// 同账号重复回调不得消费后续请求；返回领取OperationId必须与发起请求一致。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLiveOpsRequestTerminalGateTest, "GamePlatform.LiveOps.Client.RequestTerminalGate", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FLiveOpsRequestTerminalGateTest::RunTest(const FString&)
{
    FLiveOpsLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformLiveOpsClientSubsystem* Client = Fixture.Client.Get(); auto Transport = MakeShared<FLiveOpsMockTransport, ESPMode::ThreadSafe>();
    Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Gate"), Transport);
    auto OldCatalog = Transport->CatalogCompletion; OldCatalog(CatalogSnapshot(1), EGamePlatformLiveOpsError::None);
    Transport->StateCompletion(PlayerState(1), EGamePlatformLiveOpsError::None);
    Client->RefreshCatalog(); OldCatalog(CatalogSnapshot(99), EGamePlatformLiveOpsError::None);
    TestEqual(TEXT("旧目录终态不能发布新版本"), Client->GetCatalogRevision(), int64(1));
    Transport->CatalogCompletion(CatalogSnapshot(2), EGamePlatformLiveOpsError::None);
    const auto OperationA = FGuid::NewGuid(); const auto OperationB = FGuid::NewGuid();
    Client->ClaimSignIn(TEXT("Campaign"), OperationA); auto OldClaim = Transport->ClaimCompletion;
    FGamePlatformLiveOpsClaimResult Result; Result.ClaimId = TEXT("claim"); Result.ClaimOperationId = OperationB.ToString(); Result.PlayerStateRevision = 2; Result.ServerTimeUtc = FDateTime(2026,9,24,12,0,0);
    int32 Claims = 0; Client->OnClaimChanged.AddLambda([&Claims](const auto&) { ++Claims; });
    OldClaim(Result, EGamePlatformLiveOpsError::None);
    TestEqual(TEXT("错OperationId不得广播成功"), Claims, 0);
    TestEqual(TEXT("错OperationId明确InvalidResponse"), Client->GetLastError(), EGamePlatformLiveOpsError::InvalidResponse);
    TestTrue(TEXT("新领取可重新受理"), Client->ClaimSignIn(TEXT("Campaign"), OperationB));
    OldClaim(Result, EGamePlatformLiveOpsError::None);
    TestEqual(TEXT("旧终态不得消费新领取资格"), Claims, 0);
    Transport->ClaimCompletion(Result, EGamePlatformLiveOpsError::None);
    TestEqual(TEXT("新资格只完成一次"), Claims, 1);
    return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/GamePlatformLiveOpsClientTransport.h"
#include "Misc/AutomationTest.h"
#include "Services/GamePlatformLiveOpsClientSubsystem.h"

namespace
{
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
    UGamePlatformLiveOpsClientSubsystem* Client =
        NewObject<UGamePlatformLiveOpsClientSubsystem>();

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
    UGamePlatformLiveOpsClientSubsystem* Client =
        NewObject<UGamePlatformLiveOpsClientSubsystem>();

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

#endif

// 平台玩家服务Automation回归：测试Transport仅控制完成/取消，不访问生产路由；验证状态、账号隔离、广播重置与后端权威显示。
#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/GamePlatformEntitlementClientTransport.h"
#include "Misc/AutomationTest.h"
#include "Queries/GamePlatformEntitlementQuery.h"
#include "Services/GamePlatformEntitlementClientSubsystem.h"

namespace
{
class FEntitlementMockTransport final
    : public IGamePlatformEntitlementClientTransport
{
public:
    FGamePlatformEntitlementSnapshotCompletion Completion;

    virtual void CancelAllRequests() override
    {
        Completion = {};
    }

    virtual bool BeginGetSnapshot(
        FGamePlatformEntitlementSnapshotCompletion InCompletion) override
    {
        Completion = MoveTemp(InCompletion);
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformEntitlementQueryTest,
    "GamePlatform.Entitlement.Query",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformEntitlementQueryTest::RunTest(const FString&)
{
    FGamePlatformEntitlementSnapshot Snapshot;
    Snapshot.Revision = 1;
    Snapshot.GeneratedAtUtc = FDateTime::UtcNow();

    FGamePlatformEntitlementEntry Hero;
    Hero.EntitlementId = TEXT("Development.Entitlement.HeroTest");
    Hero.Category = TEXT("Hero");
    Hero.TargetType = EGamePlatformEntitlementTargetType::Hero;
    Hero.TargetId = TEXT("Development.Hero.Test");
    Hero.Status = EGamePlatformEntitlementStatus::Active;
    Snapshot.Entitlements.Add(Hero);

    TestTrue(
        TEXT("HasEntitlement命中有效权益"),
        FGamePlatformEntitlementQuery::HasEntitlement(
            Snapshot,
            Hero.EntitlementId));

    TestTrue(
        TEXT("Hero Target映射可查询"),
        FGamePlatformEntitlementQuery::HasTarget(
            Snapshot,
            EGamePlatformEntitlementTargetType::Hero,
            Hero.TargetId));

    TestTrue(
        TEXT("HasAny"),
        FGamePlatformEntitlementQuery::HasAny(
            Snapshot,
            {TEXT("Missing"), Hero.EntitlementId}));

    TestTrue(
        TEXT("HasAll"),
        FGamePlatformEntitlementQuery::HasAll(
            Snapshot,
            {Hero.EntitlementId}));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformEntitlementClientAccountTest,
    "GamePlatform.Entitlement.Client.AccountIsolation",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformEntitlementClientAccountTest::RunTest(const FString&)
{
    UGamePlatformEntitlementClientSubsystem* Client =
        NewObject<UGamePlatformEntitlementClientSubsystem>();

    TSharedPtr<FEntitlementMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FEntitlementMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("账号配置启动Snapshot请求"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-A"),
            Transport));

    FGamePlatformEntitlementSnapshotCompletion LateCompletion =
        MoveTemp(Transport->Completion);

    Client->ResetAccount();

    FGamePlatformEntitlementSnapshot Late;
    Late.Revision = 99;
    Late.GeneratedAtUtc = FDateTime::UtcNow();
    LateCompletion(
        MoveTemp(Late),
        EGamePlatformEntitlementError::None);

    TestEqual(
        TEXT("旧账号响应不能污染新状态"),
        Client->GetSnapshotRevision(),
        int64(0));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformEntitlementDerivedIndexTest,
    "GamePlatform.Entitlement.Client.DerivedIndex",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformEntitlementDerivedIndexTest::RunTest(const FString&)
{
    UGamePlatformEntitlementClientSubsystem* Client =
        NewObject<UGamePlatformEntitlementClientSubsystem>();
    TSharedPtr<FEntitlementMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FEntitlementMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("配置账号"),
        Client->ConfigureAuthenticatedAccount(TEXT("Account-Index"), Transport));

    FGamePlatformEntitlementSnapshot Snapshot;
    Snapshot.Revision = 3;
    Snapshot.GeneratedAtUtc = FDateTime::UtcNow();

    FGamePlatformEntitlementEntry Hero;
    Hero.EntitlementId = TEXT("Entitlement.Hero.One");
    Hero.TargetType = EGamePlatformEntitlementTargetType::Hero;
    Hero.TargetId = TEXT("Hero.One");
    Hero.Status = EGamePlatformEntitlementStatus::Active;
    Snapshot.Entitlements.Add(Hero);

    FGamePlatformEntitlementEntry ExpiredSkin;
    ExpiredSkin.EntitlementId = TEXT("Entitlement.Skin.Expired");
    ExpiredSkin.TargetType = EGamePlatformEntitlementTargetType::Skin;
    ExpiredSkin.TargetId = TEXT("Skin.Expired");
    ExpiredSkin.Status = EGamePlatformEntitlementStatus::Expired;
    Snapshot.Entitlements.Add(ExpiredSkin);

    FGamePlatformEntitlementSnapshotCompletion Completion =
        MoveTemp(Transport->Completion);
    Completion(MoveTemp(Snapshot), EGamePlatformEntitlementError::None);

    TestTrue(TEXT("有效权益进入索引"), Client->HasEntitlement(Hero.EntitlementId));
    TestTrue(TEXT("英雄Target进入索引"), Client->IsHeroUnlocked(Hero.TargetId));
    TestFalse(TEXT("过期皮肤不进入有效索引"), Client->IsSkinUnlocked(ExpiredSkin.TargetId));
    TestTrue(TEXT("空HasAll保持原语义=true"), Client->HasAll({}));

    const TArray<FGamePlatformEntitlementViewModel>& Views =
        Client->GetViewModelsView();
    TestEqual(TEXT("ViewModel一次构建包含全部状态"), Views.Num(), 2);
    return true;
}


// 验证公开状态监听器在Loading内重置账号：受理失败、旧传输没有请求、清空后的账号状态不被旧栈覆盖。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntitlementResetDuringStateTest, "GamePlatform.Entitlement.Client.ResetDuringState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEntitlementResetDuringStateTest::RunTest(const FString&)
{
    auto* Client = NewObject<UGamePlatformEntitlementClientSubsystem>();
    auto Transport = MakeShared<FEntitlementMockTransport, ESPMode::ThreadSafe>();
    Client->OnChanged.AddLambda([Client](auto&&...)
    {
        if (Client->GetState() == EGamePlatformEntitlementClientState::Loading) { Client->ResetAccount(); }
    });
    TestFalse(TEXT("Loading监听器重置后不得接纳旧账号请求"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Reset"), Transport));
    TestFalse(TEXT("传输未启动失效请求"), static_cast<bool>(Transport->Completion));
    TestEqual(TEXT("回调返回后保持清空状态"), Client->GetState(), EGamePlatformEntitlementClientState::Uninitialized);
    return true;
}


// 生命周期回归：测试Transport不访问网络；Reset同步通知调用Deinitialize后，关闭作用域必须拒绝恢复账号及公开刷新。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEntitlementCloseDuringConfigureTest, "GamePlatform.Entitlement.Client.CloseDuringConfigure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FEntitlementCloseDuringConfigureTest::RunTest(const FString&)
{
    auto* Client = NewObject<UGamePlatformEntitlementClientSubsystem>();
    auto Transport = MakeShared<FEntitlementMockTransport, ESPMode::ThreadSafe>();
    TestTrue(TEXT("前置账号请求成功受理"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Previous"), Transport));
    Client->OnChanged.AddLambda([Client](auto&&...) { Client->Deinitialize(); });
    TestFalse(TEXT("Reset通知内关闭后Configure不得复活服务"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Closed"), Transport));
    TestFalse(TEXT("关闭后不得启动请求"), static_cast<bool>(Transport->Completion));
    TestFalse(TEXT("公开刷新拒绝已关闭作用域"), Client->RefreshSnapshot());
    return true;
}

#endif

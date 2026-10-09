// 平台成长客户端投影回归；测试Port仅手控权威输入，验证错误/清空/轨道变更事件，不模拟生产XP写入。
#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/GamePlatformProgressionClientTransport.h"
#include "Misc/AutomationTest.h"
#include "Services/GamePlatformProgressionClientSubsystem.h"

namespace
{
class FProgressionMockTransport final
    : public IGamePlatformProgressionClientTransport
{
public:
    FGamePlatformProgressionSnapshotCompletion Completion;

    virtual void CancelAllRequests() override
    {
        Completion = {};
    }

    virtual bool BeginGetSnapshot(
        FGamePlatformProgressionSnapshotCompletion InCompletion) override
    {
        Completion = MoveTemp(InCompletion);
        return true;
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformProgressionClientAccountTest,
    "GamePlatform.Progression.Client.AccountIsolation",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformProgressionClientAccountTest::RunTest(const FString&)
{
    UGamePlatformProgressionClientSubsystem* Client =
        NewObject<UGamePlatformProgressionClientSubsystem>();

    TSharedPtr<FProgressionMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FProgressionMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("账号配置启动Snapshot请求"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-A"),
            Transport));

    FGamePlatformProgressionSnapshotCompletion Late =
        MoveTemp(Transport->Completion);

    Client->ResetAccount();

    FGamePlatformProgressionSnapshot Old;
    Old.ProgressionRevision = 99;
    Old.GeneratedAtUtc = FDateTime::UtcNow();

    Late(
        MoveTemp(Old),
        EGamePlatformProgressionError::None);

    TestEqual(
        TEXT("旧账号响应不能污染新账号"),
        Client->GetProgressionRevision(),
        int64(0));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformProgressionClientDerivedCacheTest,
    "GamePlatform.Progression.Client.DerivedCache",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformProgressionClientDerivedCacheTest::RunTest(const FString&)
{
    UGamePlatformProgressionClientSubsystem* Client =
        NewObject<UGamePlatformProgressionClientSubsystem>();

    TSharedPtr<FProgressionMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FProgressionMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("配置账号启动Snapshot请求"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-Cache"),
            Transport));

    FGamePlatformProgressionTrackState Track;
    Track.SubjectType = EGamePlatformProgressionSubjectType::Character;
    Track.SubjectId = TEXT("Character-1");
    Track.ProgressionTrackId = TEXT("Character.Level");
    Track.TotalXP = 250;
    Track.Level = 3;
    Track.MaxLevel = 10;
    Track.CurveVersion = 1;
    Track.Revision = 1;

    FGamePlatformProgressionSnapshot Snapshot;
    Snapshot.ProgressionRevision = 1;
    Snapshot.GeneratedAtUtc = FDateTime::UtcNow();
    Snapshot.Tracks.Add(Track);

    FGamePlatformProgressionSnapshotCompletion FirstCompletion =
        MoveTemp(Transport->Completion);
    FirstCompletion(
        MoveTemp(Snapshot),
        EGamePlatformProgressionError::None);

    TestEqual(
        TEXT("索引查询返回当前等级"),
        Client->GetLevel(TEXT("Character.Level"), TEXT("Character-1")),
        3);
    TestEqual(
        TEXT("索引查询返回当前XP"),
        Client->GetTotalXP(TEXT("Character.Level"), TEXT("Character-1")),
        int64(250));

    const TArray<FGamePlatformProgressionViewModel>& FirstView =
        Client->GetViewModelsView();
    TestEqual(TEXT("派生View数量"), FirstView.Num(), 1);
    TestEqual(TEXT("派生View等级"), FirstView[0].Level, 3);
    const FGamePlatformProgressionViewModel* FirstData = FirstView.GetData();

    const TArray<FGamePlatformProgressionViewModel>& SecondView =
        Client->GetViewModelsView();
    TestTrue(
        TEXT("Snapshot/Definition未变化时复用派生缓存"),
        FirstData == SecondView.GetData());

    TestTrue(TEXT("启动第二次Snapshot刷新"), Client->RefreshSnapshot());

    FGamePlatformProgressionTrackState UpdatedTrack = Track;
    UpdatedTrack.TotalXP = 420;
    UpdatedTrack.Level = 4;
    UpdatedTrack.Revision = 2;

    FGamePlatformProgressionSnapshot Updated;
    Updated.ProgressionRevision = 2;
    Updated.GeneratedAtUtc = FDateTime::UtcNow();
    Updated.Tracks.Add(UpdatedTrack);

    FGamePlatformProgressionSnapshotCompletion SecondCompletion =
        MoveTemp(Transport->Completion);
    SecondCompletion(
        MoveTemp(Updated),
        EGamePlatformProgressionError::None);

    TestEqual(
        TEXT("新Snapshot使索引缓存失效并重建"),
        Client->GetLevel(TEXT("Character.Level"), TEXT("Character-1")),
        4);
    TestEqual(
        TEXT("新Snapshot使XP查询更新"),
        Client->GetTotalXP(TEXT("Character.Level"), TEXT("Character-1")),
        int64(420));
    TestEqual(
        TEXT("新Snapshot使View缓存更新"),
        Client->GetViewModelsView()[0].Level,
        4);
    return true;
}

// 状态事件覆盖首次轨道、失败、删除/清空；测试替身仅供应领域完成，不模拟后端持久化。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionViewEventsTest, "GamePlatform.Progression.Client.ViewEvents", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProgressionViewEventsTest::RunTest(const FString&)
{
    auto* Client = NewObject<UGamePlatformProgressionClientSubsystem>();
    auto Transport = MakeShared<FProgressionMockTransport, ESPMode::ThreadSafe>();
    int32 Events = 0;
    Client->OnViewChanged.AddLambda([&]() { ++Events; });
    Client->ConfigureAuthenticatedAccount(TEXT("ViewEvents"), Transport);
    const int32 StartedEvents = Events;
    FGamePlatformProgressionSnapshot First; First.ProgressionRevision = 1; First.GeneratedAtUtc = FDateTime::UtcNow();
    FGamePlatformProgressionTrackState Track;
    Track.SubjectType = EGamePlatformProgressionSubjectType::Player; Track.SubjectId = TEXT("Player"); Track.ProgressionTrackId = TEXT("Level");
    Track.Level = 1; Track.MaxLevel = 10; Track.CurveVersion = 1; Track.Revision = 1; First.Tracks.Add(Track);
    auto Completed = MoveTemp(Transport->Completion); Completed(First, EGamePlatformProgressionError::None);
    TestTrue(TEXT("首次快照通知派生视图"), Events > StartedEvents);
    Client->RefreshSnapshot(); const int32 LoadingEvents = Events;
    auto Failed = MoveTemp(Transport->Completion); Failed({}, EGamePlatformProgressionError::BackendUnavailable);
    TestTrue(TEXT("失败状态通知"), Events > LoadingEvents);
    Client->RefreshSnapshot(); const int32 BeforeRemoved = Events;
    FGamePlatformProgressionSnapshot Empty; Empty.ProgressionRevision = 2; Empty.GeneratedAtUtc = FDateTime::UtcNow();
    auto Removed = MoveTemp(Transport->Completion); Removed(Empty, EGamePlatformProgressionError::None);
    TestTrue(TEXT("轨道删除通知"), Events > BeforeRemoved);
    const int32 BeforeReset = Events; Client->ResetAccount();
    TestTrue(TEXT("账号清空通知"), Events > BeforeReset);
    Client->OnViewChanged.Clear();
    return true;
}

// 生命周期回归：测试Transport不访问网络；Reset同步通知调用Deinitialize后，关闭作用域必须拒绝恢复账号及公开刷新。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProgressionCloseDuringConfigureTest, "GamePlatform.Progression.Client.CloseDuringConfigure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FProgressionCloseDuringConfigureTest::RunTest(const FString&)
{
    auto* Client = NewObject<UGamePlatformProgressionClientSubsystem>();
    auto Transport = MakeShared<FProgressionMockTransport, ESPMode::ThreadSafe>();
    TestTrue(TEXT("前置账号请求成功受理"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Previous"), Transport));
    Client->OnViewChanged.AddLambda([Client](auto&&...) { Client->Deinitialize(); });
    TestFalse(TEXT("Reset通知内关闭后Configure不得复活服务"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Closed"), Transport));
    TestFalse(TEXT("关闭后不得启动请求"), static_cast<bool>(Transport->Completion));
    TestFalse(TEXT("公开刷新拒绝已关闭作用域"), Client->RefreshSnapshot());
    return true;
}

#endif

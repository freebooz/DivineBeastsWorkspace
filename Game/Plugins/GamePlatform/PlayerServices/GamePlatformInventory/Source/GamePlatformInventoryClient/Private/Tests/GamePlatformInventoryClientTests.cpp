// 平台玩家服务Automation回归：测试Transport仅控制完成/取消，不访问生产路由；验证状态、账号隔离、广播重置与后端权威显示。
#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/GamePlatformInventoryClientTransport.h"
#include "Misc/AutomationTest.h"
#include "Services/GamePlatformInventoryClientSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
/**
 * 仅Automation的合法Outer夹具：LocalPlayer的Within是Engine，领域Subsystem的Within是LocalPlayer。
 * GT显式构造并强持有两者，不PlayerAdded/不建World或自动登录；原测试Transport/账号前提保持。
 * 无Viewport时GetGameInstance为nullptr，不能把本夹具当完整GI/Online装配或生产服务。
 * 任意正常/提前返回先Deinitialize清委托/取消请求，再释放Client与Player，避免GC和测试间残留。
 */
struct FInventoryLocalPlayerFixture
{
    TStrongObjectPtr<ULocalPlayer> Player;
    TStrongObjectPtr<UGamePlatformInventoryClientSubsystem> Client;
    bool Initialize(FAutomationTestBase& Test)
    {
        if (!Test.TestNotNull(TEXT("LocalPlayer真实Engine Within宿主"), GEngine)) return false;
        Player.Reset(NewObject<ULocalPlayer>(GEngine));
        if (!Test.TestNotNull(TEXT("领域Subsystem真实LocalPlayer Outer"), Player.Get())) return false;
        Client.Reset(NewObject<UGamePlatformInventoryClientSubsystem>(Player.Get()));
        return Test.TestNotNull(TEXT("合法Outer的领域Subsystem实例"), Client.Get());
    }
    ~FInventoryLocalPlayerFixture()
    {
        if (Client.IsValid()) Client->Deinitialize();
        Client.Reset();
        Player.Reset();
    }
};

class FInventoryMockTransport final
    : public IGamePlatformInventoryClientTransport
{
public:
    FGamePlatformInventorySnapshotCompletion SnapshotCompletion;
    FGamePlatformInventoryMutationCompletion MutationCompletion;
    bool bSynchronousMoveFailure = false;
    int32 SnapshotStarts = 0;
    int32 QueryStarts = 0;
    int32 MoveStarts = 0;

    virtual void CancelAllRequests() override
    {
        SnapshotCompletion = {};
        MutationCompletion = {};
    }

    virtual bool BeginGetSnapshot(
        FGamePlatformInventorySnapshotCompletion Completion) override
    {
        ++SnapshotStarts;
        SnapshotCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginGetOperation(
        const FGuid&,
        FGamePlatformInventoryMutationCompletion Completion) override
    {
        ++QueryStarts;
        MutationCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginMove(
        const FGamePlatformInventoryMoveRequest&,
        FGamePlatformInventoryMutationCompletion Completion) override
    {
        ++MoveStarts;
        MutationCompletion = MoveTemp(Completion);
        if (bSynchronousMoveFailure) { MutationCompletion({}, EGamePlatformInventoryError::OutcomeUnknown); }
        return true;
    }

    virtual bool BeginSplit(
        const FGamePlatformInventorySplitRequest&,
        FGamePlatformInventoryMutationCompletion Completion) override
    {
        MutationCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginMerge(
        const FGamePlatformInventoryMergeRequest&,
        FGamePlatformInventoryMutationCompletion Completion) override
    {
        MutationCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginSetQuickbar(
        const FGamePlatformInventoryQuickbarRequest&,
        FGamePlatformInventoryMutationCompletion Completion) override
    {
        MutationCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginClearQuickbar(
        const FGamePlatformInventoryQuickbarRequest&,
        FGamePlatformInventoryMutationCompletion Completion) override
    {
        MutationCompletion = MoveTemp(Completion);
        return true;
    }

    void CompleteSnapshot(int64 Revision)
    {
        FGamePlatformInventorySnapshot Snapshot;
        Snapshot.InventoryRevision = Revision;
        FGamePlatformInventorySnapshotCompletion Completion =
            MoveTemp(SnapshotCompletion);
        Completion(
            MoveTemp(Snapshot),
            EGamePlatformInventoryError::None);
    }

    void CompleteMutation(
        const FGuid& OperationId,
        int64 Revision,
        EGamePlatformInventoryError Error)
    {
        FGamePlatformInventoryMutationResult Result;
        Result.OperationId = OperationId;
        Result.Snapshot.InventoryRevision = Revision;

        FGamePlatformInventoryMutationCompletion Completion =
            MoveTemp(MutationCompletion);

        Completion(MoveTemp(Result), Error);
    }
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInventoryClientMutationTest,
    "GamePlatform.Inventory.Client.MutationAndRevision",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformInventoryClientMutationTest::RunTest(
    const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get();

    TSharedPtr<FInventoryMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("认证账号配置启动Snapshot读取"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-A"),
            Transport));

    TestEqual(
        TEXT("读取期间状态为Loading"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Loading);

    TestFalse(
        TEXT("同账号Snapshot在途时拒绝重复Refresh"),
        Client->RefreshSnapshot());

    Transport->CompleteSnapshot(5);

    TestEqual(
        TEXT("Snapshot完成后Ready"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Ready);
    TestEqual(
        TEXT("Revision=5"),
        Client->GetInventoryRevision(),
        int64(5));

    const FGuid OperationId =
        Client->RequestMove(
            TEXT("00000000-0000-0000-0000-000000000001"),
            TEXT("main"),
            2);

    TestTrue(TEXT("Move生成OperationId"), OperationId.IsValid());
    TestTrue(TEXT("Move进入Pending"), Client->HasPendingOperation());

    Transport->CompleteMutation(
        OperationId,
        6,
        EGamePlatformInventoryError::None);

    TestEqual(
        TEXT("成功Mutation更新Revision"),
        Client->GetInventoryRevision(),
        int64(6));
    TestFalse(
        TEXT("成功后清理Pending"),
        Client->HasPendingOperation());

    const FGuid ConflictOperation =
        Client->RequestMove(
            TEXT("00000000-0000-0000-0000-000000000001"),
            TEXT("main"),
            3);

    Transport->CompleteMutation(
        ConflictOperation,
        0,
        EGamePlatformInventoryError::RevisionConflict);

    TestEqual(
        TEXT("Revision冲突进入Reconciling"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Reconciling);

    Transport->CompleteSnapshot(7);

    TestEqual(
        TEXT("对账后使用较新Revision"),
        Client->GetInventoryRevision(),
        int64(7));
    TestEqual(
        TEXT("对账后恢复Ready"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Ready);
    TestFalse(
        TEXT("冲突对账后清理旧Pending"),
        Client->HasPendingOperation());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInventoryClientAccountGenerationTest,
    "GamePlatform.Inventory.Client.AccountGeneration",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformInventoryClientAccountGenerationTest::RunTest(
    const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get();

    TSharedPtr<FInventoryMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("配置账号"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-A"),
            Transport));

    FGamePlatformInventorySnapshotCompletion LateCompletion =
        MoveTemp(Transport->SnapshotCompletion);

    Client->ResetAccount();

    FGamePlatformInventorySnapshot LateSnapshot;
    LateSnapshot.InventoryRevision = 99;
    LateCompletion(
        MoveTemp(LateSnapshot),
        EGamePlatformInventoryError::None);

    TestEqual(
        TEXT("旧账号迟到响应不能污染新状态"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Uninitialized);
    TestEqual(
        TEXT("旧账号迟到响应不能写入Revision"),
        Client->GetInventoryRevision(),
        int64(0));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInventoryClientOperationRecoveryTest,
    "GamePlatform.Inventory.Client.OperationRecovery",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformInventoryClientOperationRecoveryTest::RunTest(
    const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get();

    TSharedPtr<FInventoryMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("配置账号"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-Recovery"),
            Transport));

    Transport->CompleteSnapshot(10);

    const FGuid OperationId =
        Client->RequestMove(
            TEXT("00000000-0000-0000-0000-000000000001"),
            TEXT("main"),
            4);

    TestTrue(TEXT("生成OperationId"), OperationId.IsValid());

    Transport->CompleteMutation(
        OperationId,
        0,
        EGamePlatformInventoryError::BackendUnavailable);

    TestEqual(
        TEXT("网络结果未知后进入Error并保留Pending"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Error);
    TestTrue(
        TEXT("结果未知保留同一Operation"),
        Client->HasPendingOperation());

    TestTrue(
        TEXT("显式重试先查询OperationId"),
        Client->RetryPendingOperation());

    TestFalse(
        TEXT("Operation查询期间普通Refresh不能清理未决操作"),
        Client->RefreshSnapshot());

    Transport->CompleteMutation(
        OperationId,
        0,
        EGamePlatformInventoryError::OperationNotFound);

    TestEqual(
        TEXT("确认未落库后复用同OperationId重发"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Mutating);

    Transport->CompleteMutation(
        OperationId,
        11,
        EGamePlatformInventoryError::None);

    TestEqual(
        TEXT("幂等重发成功后Revision更新"),
        Client->GetInventoryRevision(),
        int64(11));
    TestFalse(
        TEXT("恢复成功后清理Pending"),
        Client->HasPendingOperation());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInventoryDerivedCacheTest,
    "GamePlatform.Inventory.Client.DerivedCache",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformInventoryDerivedCacheTest::RunTest(const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get();
    TSharedPtr<FInventoryMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("配置账号"),
        Client->ConfigureAuthenticatedAccount(TEXT("Account-Cache"), Transport));

    FGamePlatformInventorySnapshot Snapshot;
    Snapshot.InventoryRevision = 7;

    FGamePlatformInventoryItemInstance Later;
    Later.ItemInstanceId = TEXT("Item-B");
    Later.ItemDefinitionId = TEXT("Item.Definition.B");
    Later.Quantity = 1;
    Later.ContainerId = TEXT("main");
    Later.SlotIndex = 5;
    Later.Revision = 1;

    FGamePlatformInventoryItemInstance Earlier;
    Earlier.ItemInstanceId = TEXT("Item-A");
    Earlier.ItemDefinitionId = TEXT("Item.Definition.A");
    Earlier.Quantity = 2;
    Earlier.MaxStackSize = 20;
    Earlier.ContainerId = TEXT("main");
    Earlier.SlotIndex = 1;
    Earlier.Revision = 1;

    Snapshot.Items = {Later, Earlier};
    FGamePlatformInventorySnapshotCompletion Completion =
        MoveTemp(Transport->SnapshotCompletion);
    Completion(MoveTemp(Snapshot), EGamePlatformInventoryError::None);

    const TArray<FGamePlatformInventoryItemInstance>& Sorted =
        Client->GetSortedItemsView();
    TestEqual(TEXT("排序缓存条数"), Sorted.Num(), 2);
    TestEqual(TEXT("按槽位排序"), Sorted[0].ItemInstanceId, FString(TEXT("Item-A")));
    TestNotNull(TEXT("索引可O(1)查找Item-B"), Client->FindItem(TEXT("Item-B")));

    const FGuid MoveId = Client->RequestMove(TEXT("Item-B"), TEXT("main"), 2);
    TestTrue(TEXT("Move请求有效"), MoveId.IsValid());

    const TArray<FGamePlatformInventoryItemViewModel>& Views =
        Client->GetViewModelsView();
    const FGamePlatformInventoryItemViewModel* PendingView =
        Views.FindByPredicate([](const FGamePlatformInventoryItemViewModel& View)
        {
            return View.ItemInstanceId == TEXT("Item-B");
        });
    TestNotNull(TEXT("找到Item-B ViewModel"), PendingView);
    if (PendingView)
    {
        TestTrue(TEXT("Pending操作进入派生缓存"), PendingView->bPending);
    }

    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInventorySnapshotIntegrityTest,
    "GamePlatform.Inventory.Client.SnapshotIntegrity",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformInventorySnapshotIntegrityTest::RunTest(
    const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get();
    TSharedPtr<FInventoryMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("配置完整性测试账号"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-Integrity"),
            Transport));

    FGamePlatformInventorySnapshot Snapshot;
    Snapshot.InventoryRevision = 1;

    FGamePlatformInventoryItemInstance First;
    First.ItemInstanceId = TEXT("Duplicate-Item");
    First.ItemDefinitionId = TEXT("Item.Definition.A");
    First.Quantity = 1;
    First.ContainerId = TEXT("main");
    First.SlotIndex = 0;
    First.Revision = 1;

    FGamePlatformInventoryItemInstance Duplicate = First;
    Duplicate.SlotIndex = 1;
    Snapshot.Items = {First, Duplicate};

    FGamePlatformInventorySnapshotCompletion Completion =
        MoveTemp(Transport->SnapshotCompletion);
    Completion(
        MoveTemp(Snapshot),
        EGamePlatformInventoryError::None);

    TestEqual(
        TEXT("重复ItemInstanceId必须拒绝"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Error);
    TestEqual(
        TEXT("重复实例返回InvalidResponse"),
        Client->GetLastError(),
        EGamePlatformInventoryError::InvalidResponse);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInventoryDeterministicErrorTest,
    "GamePlatform.Inventory.Client.DeterministicError",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformInventoryDeterministicErrorTest::RunTest(
    const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get();
    TSharedPtr<FInventoryMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("配置业务错误测试账号"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-Error"),
            Transport));
    Transport->CompleteSnapshot(3);

    const FGuid OperationId =
        Client->RequestMove(
            TEXT("Item-A"),
            TEXT("main"),
            1);
    TestTrue(TEXT("生成操作编号"), OperationId.IsValid());

    Transport->CompleteMutation(
        OperationId,
        0,
        EGamePlatformInventoryError::SlotOccupied);

    TestEqual(
        TEXT("确定性业务错误后恢复Ready允许重新操作"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Ready);
    TestEqual(
        TEXT("保留稳定业务错误供UI展示"),
        Client->GetLastError(),
        EGamePlatformInventoryError::SlotOccupied);
    TestFalse(
        TEXT("确定性错误不会保留结果未知Pending"),
        Client->HasPendingOperation());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGamePlatformInventorySnapshotSingleFlightTest,
    "GamePlatform.Inventory.Client.SnapshotSingleFlightAndPendingIsolation",
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::EngineFilter)

bool FGamePlatformInventorySnapshotSingleFlightTest::RunTest(
    const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get();
    TSharedPtr<FInventoryMockTransport, ESPMode::ThreadSafe> Transport =
        MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();

    TestTrue(
        TEXT("配置账号后启动首个快照请求"),
        Client->ConfigureAuthenticatedAccount(
            TEXT("Account-SingleFlight"),
            Transport));
    TestFalse(
        TEXT("已有快照请求在途时拒绝第二个刷新"),
        Client->RefreshSnapshot());

    Transport->CompleteSnapshot(5);
    TestEqual(
        TEXT("首个快照完成后进入Ready"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Ready);

    TestTrue(
        TEXT("Ready状态允许显式刷新"),
        Client->RefreshSnapshot());
    TestFalse(
        TEXT("显式刷新仍保持单飞"),
        Client->RefreshSnapshot());
    Transport->CompleteSnapshot(6);

    const FGuid OperationId = Client->RequestMove(
        TEXT("Item-Pending"),
        TEXT("main"),
        1);
    TestTrue(TEXT("写操作生成OperationId"), OperationId.IsValid());
    TestFalse(
        TEXT("Mutation在途期间禁止普通快照刷新"),
        Client->RefreshSnapshot());

    Transport->CompleteMutation(
        OperationId,
        0,
        EGamePlatformInventoryError::BackendUnavailable);
    TestEqual(
        TEXT("结果未知后进入Error"),
        Client->GetState(),
        EGamePlatformInventoryClientState::Error);
    TestTrue(
        TEXT("结果未知必须保留原Pending Operation"),
        Client->HasPendingOperation());
    TestFalse(
        TEXT("结果未知Pending存在时普通刷新不得清理操作"),
        Client->RefreshSnapshot());

    return true;
}


// 验证公开状态监听器在Loading内重置账号：受理失败、旧传输没有请求、清空后的账号状态不被旧栈覆盖。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryResetDuringStateTest, "GamePlatform.Inventory.Client.ResetDuringState", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryResetDuringStateTest::RunTest(const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get();
    auto Transport = MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();
    Client->OnChanged.AddLambda([Client](auto&&...)
    {
        if (Client->GetState() == EGamePlatformInventoryClientState::Loading) { Client->ResetAccount(); }
    });
    TestFalse(TEXT("Loading监听器重置后不得接纳旧账号请求"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Reset"), Transport));
    TestFalse(TEXT("传输未启动失效请求"), static_cast<bool>(Transport->SnapshotCompletion));
    TestEqual(TEXT("回调返回后保持清空状态"), Client->GetState(), EGamePlatformInventoryClientState::Uninitialized);
    return true;
}


// 生命周期回归：测试Transport不访问网络；Reset同步通知调用Deinitialize后，关闭作用域必须拒绝恢复账号及公开刷新。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryCloseDuringConfigureTest, "GamePlatform.Inventory.Client.CloseDuringConfigure", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryCloseDuringConfigureTest::RunTest(const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get();
    auto Transport = MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();
    TestTrue(TEXT("前置账号请求成功受理"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Previous"), Transport));
    Client->OnChanged.AddLambda([Client](auto&&...) { Client->Deinitialize(); });
    TestFalse(TEXT("Reset通知内关闭后Configure不得复活服务"), Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Closed"), Transport));
    TestFalse(TEXT("关闭后不得启动请求"), static_cast<bool>(Transport->SnapshotCompletion));
    TestFalse(TEXT("公开刷新拒绝已关闭作用域"), Client->RefreshSnapshot());
    return true;
}


// 同步真实Transport回调不得被BeginMove返回栈覆盖；旧终态也不能消耗下一次持久结果查询。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventorySynchronousOutcomeTest, "GamePlatform.Inventory.Client.SynchronousOutcome", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventorySynchronousOutcomeTest::RunTest(const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get(); auto Transport = MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();
    Client->ConfigureAuthenticatedAccount(TEXT("Fixture-Sync"), Transport); Transport->CompleteSnapshot(1);
    Transport->bSynchronousMoveFailure = true;
    const auto OperationId = Client->RequestMove(TEXT("Fixture-Item"), TEXT("main"), 1);
    TestTrue(TEXT("请求保留操作身份"), OperationId.IsValid());
    TestEqual(TEXT("同步未知结果保持Error"), Client->GetState(), EGamePlatformInventoryClientState::Error);
    auto Old = Transport->MutationCompletion; Client->RetryPendingOperation();
    Old({}, EGamePlatformInventoryError::BackendUnavailable);
    TestEqual(TEXT("旧Mutation终态不得覆盖新查询"), Client->GetState(), EGamePlatformInventoryClientState::Reconciling);
    return true;
}


// 操作查询A的未知结果通知允许监听器启动查询B；旧A返回栈不能自动重发Mutation并覆盖B。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryQueryListenerTakeoverTest, "GamePlatform.Inventory.Client.QueryListenerTakeover", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryQueryListenerTakeoverTest::RunTest(const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get(); auto Transport = MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();
    Client->ConfigureAuthenticatedAccount(TEXT("Fixture-QueryTakeover"), Transport); Transport->CompleteSnapshot(1);
    const auto OperationId = Client->RequestMove(TEXT("Fixture-Item"), TEXT("main"), 1);
    Transport->CompleteMutation(OperationId, 0, EGamePlatformInventoryError::BackendUnavailable);
    Client->RetryPendingOperation(); auto QueryA = Transport->MutationCompletion;
    bool bTakenOver = false; bool bQueryBAccepted = false;
    Client->OnChanged.AddLambda([Client, &bTakenOver, &bQueryBAccepted]()
    {
        if (!bTakenOver && Client->GetLastError() == EGamePlatformInventoryError::OutcomeUnknown)
        { bTakenOver = true; bQueryBAccepted = Client->RetryPendingOperation(); }
    });
    QueryA({}, EGamePlatformInventoryError::OperationNotFound);
    TestTrue(TEXT("通知监听器接管查询B"), bQueryBAccepted);
    TestEqual(TEXT("查询A旧栈不得重发Mutation"), Transport->MoveStarts, 1);
    TestEqual(TEXT("只受理A/B两个持久结果查询"), Transport->QueryStarts, 2);
    TestEqual(TEXT("查询B保持Reconciling"), Client->GetState(), EGamePlatformInventoryClientState::Reconciling);
    Transport->CompleteMutation(OperationId, 2, EGamePlatformInventoryError::None);
    TestEqual(TEXT("B终态可以完成并恢复Ready"), Client->GetState(), EGamePlatformInventoryClientState::Ready);
    QueryA({}, EGamePlatformInventoryError::OperationNotFound);
    TestEqual(TEXT("A重复终态不得发新Mutation"), Transport->MoveStarts, 1);
    Client->Deinitialize(); return true;
}

// RevisionConflict通知监听器先启动Snapshot对账；旧Mutation栈不得再次刷新并误报BackendUnavailable。
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FInventoryConflictListenerTakeoverTest, "GamePlatform.Inventory.Client.ConflictListenerTakeover", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FInventoryConflictListenerTakeoverTest::RunTest(const FString&)
{
    FInventoryLocalPlayerFixture Fixture;
    if (!Fixture.Initialize(*this)) return false;
    UGamePlatformInventoryClientSubsystem* Client = Fixture.Client.Get(); auto Transport = MakeShared<FInventoryMockTransport, ESPMode::ThreadSafe>();
    Client->ConfigureAuthenticatedAccount(TEXT("Fixture-ConflictTakeover"), Transport); Transport->CompleteSnapshot(1);
    const auto OperationId = Client->RequestMove(TEXT("Fixture-Item"), TEXT("main"), 1);
    bool bTakenOver = false; bool bSnapshotAccepted = false; int32 UnavailableEvents = 0;
    Client->OnChanged.AddLambda([Client, &bTakenOver, &bSnapshotAccepted, &UnavailableEvents]()
    {
        if (Client->GetLastError() == EGamePlatformInventoryError::BackendUnavailable) { ++UnavailableEvents; }
        if (!bTakenOver && Client->GetLastError() == EGamePlatformInventoryError::RevisionConflict)
        { bTakenOver = true; bSnapshotAccepted = Client->RefreshSnapshot(); }
    });
    Transport->CompleteMutation(OperationId, 0, EGamePlatformInventoryError::RevisionConflict);
    TestTrue(TEXT("监听器对账Snapshot已受理"), bSnapshotAccepted);
    TestEqual(TEXT("总计仅初始化及对账两次Snapshot"), Transport->SnapshotStarts, 2);
    TestEqual(TEXT("对账保持Reconciling"), Client->GetState(), EGamePlatformInventoryClientState::Reconciling);
    TestEqual(TEXT("接管后不得出现伪BackendUnavailable"), UnavailableEvents, 0);
    TestEqual(TEXT("保留真实RevisionConflict待终态"), Client->GetLastError(), EGamePlatformInventoryError::RevisionConflict);
    Transport->CompleteSnapshot(2);
    TestFalse(TEXT("对账成功释放旧Pending资格"), Client->HasPendingOperation());
    TestEqual(TEXT("对账成功恢复Ready"), Client->GetState(), EGamePlatformInventoryClientState::Ready);
    Client->Deinitialize(); return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "Interfaces/GamePlatformInventoryClientTransport.h"
#include "Misc/AutomationTest.h"
#include "Services/GamePlatformInventoryClientSubsystem.h"

namespace
{
class FInventoryMockTransport final
    : public IGamePlatformInventoryClientTransport
{
public:
    FGamePlatformInventorySnapshotCompletion SnapshotCompletion;
    FGamePlatformInventoryMutationCompletion MutationCompletion;

    virtual void CancelAllRequests() override
    {
        SnapshotCompletion = {};
        MutationCompletion = {};
    }

    virtual bool BeginGetSnapshot(
        FGamePlatformInventorySnapshotCompletion Completion) override
    {
        SnapshotCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginGetOperation(
        const FGuid&,
        FGamePlatformInventoryMutationCompletion Completion) override
    {
        MutationCompletion = MoveTemp(Completion);
        return true;
    }

    virtual bool BeginMove(
        const FGamePlatformInventoryMoveRequest&,
        FGamePlatformInventoryMutationCompletion Completion) override
    {
        MutationCompletion = MoveTemp(Completion);
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
    UGamePlatformInventoryClientSubsystem* Client =
        NewObject<UGamePlatformInventoryClientSubsystem>();

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
    UGamePlatformInventoryClientSubsystem* Client =
        NewObject<UGamePlatformInventoryClientSubsystem>();

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
    UGamePlatformInventoryClientSubsystem* Client =
        NewObject<UGamePlatformInventoryClientSubsystem>();

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
    UGamePlatformInventoryClientSubsystem* Client =
        NewObject<UGamePlatformInventoryClientSubsystem>();
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
    UGamePlatformInventoryClientSubsystem* Client =
        NewObject<UGamePlatformInventoryClientSubsystem>();
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
    UGamePlatformInventoryClientSubsystem* Client =
        NewObject<UGamePlatformInventoryClientSubsystem>();
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
    UGamePlatformInventoryClientSubsystem* Client =
        NewObject<UGamePlatformInventoryClientSubsystem>();
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

#endif

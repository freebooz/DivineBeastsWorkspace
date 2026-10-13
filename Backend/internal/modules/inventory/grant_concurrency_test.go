package inventory

// 多GameServer事件的并发幂等与乐观锁：同一事实同时抵达时只发一次，
// 不同事实竞争同一玩家背包修订时一方收到冲突，保留原事件身份重试后应完整发放。
// 该测试使用真实内存仓储写锁与领域服务，不假称完成PostgreSQL高并发压测。

import (
    "context"
    "sync"
    "testing"

    "divinebeasts/backend/internal/platform/apperror"
)

func TestGrantSameTrustedEventConcurrentReplaysOnlyOnce(t *testing.T) {
    const playerID = "player-same-trusted-event"
    ctx := context.Background()
    repo := NewMemoryRepository()
    service := NewTrustedGrantService(repo,
        testGrantAuthorizer{playerID: playerID, rule: GrantRule{MaxStackSize: 20}}, nil)
    original, err := repo.GetSnapshot(ctx, playerID)
    if err != nil { t.Fatal(err) }
    command := trustedGrant(playerID, "drop-source-unique-101", original.InventoryRevision, 5)
    type outcome struct{ value MutationResult; err error }
    responses := make(chan outcome, 2)
    start := make(chan struct{})
    var wg sync.WaitGroup
    for index := 0; index < 2; index++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            <-start
            value, err := service.GrantTrusted(ctx, playerID, command)
            responses <- outcome{value, err}
        }()
    }
    close(start)
    wg.Wait()
    close(responses)
    firstCommit, duplicates := 0, 0
    for result := range responses {
        if result.err != nil { t.Fatalf("相同事件并发重试应从幂等记录回放: %v", result.err) }
        if result.value.Duplicate { duplicates++ } else { firstCommit++ }
    }
    after, err := repo.GetSnapshot(ctx, playerID)
    if err != nil { t.Fatal(err) }
    if firstCommit != 1 || duplicates != 1 ||
        after.InventoryRevision != original.InventoryRevision + 1 ||
        len(after.Items) != 1 || after.Items[0].Quantity != 5 {
        t.Fatalf("相同事件并发造成了重复发奖: first=%d replay=%d state=%+v",
            firstCommit, duplicates, after)
    }
}

func TestGrantDistinctTrustedEventsRevisionConflictThenRetry(t *testing.T) {
    const playerID = "player-two-concurrent-drops"
    ctx := context.Background()
    repo := NewMemoryRepository()
    service := NewTrustedGrantService(repo,
        testGrantAuthorizer{playerID: playerID, rule: GrantRule{MaxStackSize: 20}}, nil)
    before, _ := repo.GetSnapshot(ctx, playerID)
    commands := []GrantCommand{
        trustedGrant(playerID, "drop-event-a", before.InventoryRevision, 4),
        trustedGrant(playerID, "drop-event-b", before.InventoryRevision, 6),
    }
    type outcome struct {
        index int
        result MutationResult
        err error
    }
    replies := make(chan outcome, 2)
    start := make(chan struct{})
    var wg sync.WaitGroup
    for index, command := range commands {
        wg.Add(1)
        go func(i int, cmd GrantCommand) {
            defer wg.Done()
            <-start
            result, err := service.GrantTrusted(ctx, playerID, cmd)
            replies <- outcome{i, result, err}
        }(index, command)
    }
    close(start)
    wg.Wait()
    close(replies)

    successIndex, conflictIndex := -1, -1
    for reply := range replies {
        switch {
        case reply.err == nil:
            if successIndex != -1 { t.Fatal("同一Revision的两个不同事件不允许都直接成功") }
            successIndex = reply.index
        default:
            appErr, ok := reply.err.(*apperror.Error)
            if !ok || appErr.Code != "INVENTORY_REVISION_CONFLICT" {
                t.Fatalf("不同事件竞争必须返回修订号冲突而非静默丢失: %v", reply.err)
            }
            if conflictIndex != -1 { t.Fatal("只允许一个不同事件出现并发冲突") }
            conflictIndex = reply.index
        }
    }
    if successIndex < 0 || conflictIndex < 0 { t.Fatal("并发任务未产生一个成功和一个冲突") }

    snapshot, err := repo.GetSnapshot(ctx, playerID)
    if err != nil { t.Fatal(err) }
    if snapshot.InventoryRevision != before.InventoryRevision+1 ||
        len(snapshot.Items) != 1 || snapshot.Items[0].Quantity != commands[successIndex].Quantity {
        t.Fatalf("修订冲突前不能部分写入另一个事件: %+v", snapshot)
    }

    retry := commands[conflictIndex]
    retry.ExpectedRevision = snapshot.InventoryRevision
    applied, err := service.GrantTrusted(ctx, playerID, retry)
    if err != nil || applied.Duplicate { t.Fatalf("原来源事件按新Revision重试应首次完整发放: %v %+v", err, applied) }
    finalState, _ := repo.GetSnapshot(ctx, playerID)
    if finalState.InventoryRevision != before.InventoryRevision+2 ||
        len(finalState.Items) != 1 || finalState.Items[0].Quantity != 10 {
        t.Fatalf("两个不同采集事件最终入包数量应完全相加: %+v", finalState)
    }
    replay, err := service.GrantTrusted(ctx, playerID, retry)
    if err != nil || !replay.Duplicate {
        t.Fatalf("重试成功后再次请求应稳定幂等: %v %+v", err, replay)
    }
}

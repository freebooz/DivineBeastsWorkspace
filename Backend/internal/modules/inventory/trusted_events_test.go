package inventory

// 受控事件处理器测试：事件不可变、服务器会话世代不可伪造，可信来源重放不二次发奖。
// 仅为纯Go领域用例，不把测试验证器当作真实Dedicated Server准入系统。

import (
	"context"
	"errors"
	"sync"
	"testing"
)

type memoryConfirmedJournal struct {
	mu sync.Mutex
	items map[string]ConfirmedInventoryEvent
}

func (m *memoryConfirmedJournal) RecordConfirmedEvent(_ context.Context,e ConfirmedInventoryEvent)error{
	m.mu.Lock();defer m.mu.Unlock()
	if m.items==nil {m.items=map[string]ConfirmedInventoryEvent{}}
	if old,ok:=m.items[e.SourceEventID];ok {
		old.ExpectedRevision=e.ExpectedRevision
		if old!=e{return errors.New("同事件编号无法重写奖励内容")}
		return nil
	}
	m.items[e.SourceEventID]=e
	return nil
}

func TestTrustedEventProcessorGrantAndLeaseFence(t *testing.T){
	ctx:=context.Background()
	const playerID="event-player"
	repo:=NewMemoryRepository()
	auth:=testGrantAuthorizer{playerID:playerID,rule:GrantRule{MaxStackSize:20}}
	svc:=NewTrustedGrantService(repo,auth,nil)
	journal:=&memoryConfirmedJournal{}
	processor,err:=NewTrustedEventProcessor("divine-beasts",
		fakeVerifiedServerPlayer{playerID,"world-village-1","boot-20261013"},journal,svc)
	if err!=nil{t.Fatal(err)}
	cmd:=trustedGrant(playerID,"resource-node-1:drop-1",1,3)
	e:=ConfirmedInventoryEvent{
		EventKind:TrustedEventGrant, GameID:cmd.GameID,
		GameServerID:cmd.GameServerID,ServerBootID:cmd.ServerBootID,
		PlayerID:playerID,SessionEpoch:cmd.SessionEpoch,ConnectionID:cmd.ConnectionID,
		SourceEventID:cmd.SourceEventID,ExpectedRevision:cmd.ExpectedRevision,
		ItemDefinitionID:cmd.ItemDefinitionID,Quantity:cmd.Quantity,ReasonCode:cmd.ReasonCode,
	}
	for _,bad:=range []ConfirmedInventoryEvent{
		func() ConfirmedInventoryEvent{x:=e;x.SessionEpoch=4;return x}(),
		func() ConfirmedInventoryEvent{x:=e;x.ConnectionID="old-connection";return x}(),
		func() ConfirmedInventoryEvent{x:=e;x.GameID="another-game";return x}(),
		func() ConfirmedInventoryEvent{x:=e;x.ItemInstanceID="client-forged-id";return x}(),
	}{
		if _,err=processor.Process(ctx,bad);err==nil{
			t.Fatalf("事件租约/权限门禁拒绝失败: %+v",bad)
		}
	}
	if len(journal.items)!=0{t.Fatal("未认证事件不能被持久接受")}
	first,err:=processor.Process(ctx,e)
	if err!=nil || first.Duplicate || len(first.Snapshot.Items)!=1 ||
		first.Snapshot.Items[0].Quantity!=3 {
		t.Fatalf("可信拾取入包失败: %+v %v",first,err)
	}
	replayed,err:=processor.Process(ctx,e)
	if err!=nil || !replayed.Duplicate ||
		replayed.Snapshot.InventoryRevision!=first.Snapshot.InventoryRevision{
		t.Fatalf("同一资源掉落事件重复投递不应二次发奖: %+v %v",replayed,err)
	}
	// 同事件的乐观版本提示变化不会创造第二份奖励，幂等身份只取稳定事实字段。
	newRevisionHint:=e;newRevisionHint.ExpectedRevision=first.Snapshot.InventoryRevision
	replayedAfterMove,err:=processor.Process(ctx,newRevisionHint)
	if err!=nil || !replayedAfterMove.Duplicate ||
		replayedAfterMove.Snapshot.InventoryRevision!=first.Snapshot.InventoryRevision {
		t.Fatalf("相同事实调整版本提示不能再次发奖: %+v %v",replayedAfterMove,err)
	}
	changed:=e;changed.Quantity=4
	if _,err=processor.Process(ctx,changed);err==nil{
		t.Fatal("来源事件已记录后不能更改数量以重复领奖")
	}
	if len(journal.items)!=1{t.Fatal("可信事件日志应只有单一来源记录")}
}

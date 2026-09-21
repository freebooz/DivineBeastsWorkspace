package party

import "testing"

func TestLockedRosterRejectsMemberChanges(t *testing.T) {
	p := New("party-1", Member{PlayerID: "leader", DisplayName: "队长"})
	if err := p.AddMember(Member{PlayerID: "p2", DisplayName: "玩家2"}); err != nil {
		t.Fatal(err)
	}
	p.LockRoster()
	if err := p.AddMember(Member{PlayerID: "p3"}); err == nil {
		t.Fatal("锁定名单后禁止加入成员")
	}
	if err := p.RemoveMember("p2"); err == nil {
		t.Fatal("锁定名单后禁止移除成员")
	}
}

func TestLeaderCanSelectArenaModeBeforeQueueing(t *testing.T) {
	p := New("party-1", Member{PlayerID: "leader"})
	if err := p.SelectArenaMode("leader", "Arena.Mode.Team3v3"); err != nil {
		t.Fatalf("选择竞技模式失败: %v", err)
	}
	if p.SelectedArenaModeID != "Arena.Mode.Team3v3" {
		t.Fatal("竞技模式未保存")
	}
}

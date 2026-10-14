// 本文件验证普通队伍名单冻结与队长模式选择；仅为纯领域回归，不证明生产仓储、认证或联机闭环。
package party

import (
	"strings"
	"testing"
)

// TestLockedRosterRejectsMemberChanges验证有效队伍锁定后，加入和移除均不能绕过名单锁。
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

// TestLeaderCanSelectArenaModeBeforeQueueing验证入队前可保存队长选择；不在本测试复制项目模式目录。
func TestLeaderCanSelectArenaModeBeforeQueueing(t *testing.T) {
	p := New("party-1", Member{PlayerID: "leader"})
	if err := p.SelectArenaMode("leader", "Arena.Mode.Team3v3"); err != nil {
		t.Fatalf("选择竞技模式失败: %v", err)
	}
	if p.SelectedArenaModeID != "Arena.Mode.Team3v3" {
		t.Fatal("竞技模式未保存")
	}
}

// TestPublicPartyRuleErrorsHaveStableCodes验证普通权限／命令错误具有公开适配白名单可识别的前缀，不伪装为服务故障。
func TestPublicPartyRuleErrorsHaveStableCodes(t *testing.T) {
	p := New("party-a", Member{PlayerID: "leader"})
	if err := p.SetReady("outsider", true); err == nil || !strings.HasPrefix(err.Error(), "PARTY_NOT_MEMBER:") {
		t.Fatalf("非成员准备应返回PARTY_NOT_MEMBER，实际：%v", err)
	}
	if err := p.RemoveMember("leader"); err == nil || !strings.HasPrefix(err.Error(), "PARTY_COMMAND_INVALID:") {
		t.Fatalf("移除队长应返回PARTY_COMMAND_INVALID，实际：%v", err)
	}
}

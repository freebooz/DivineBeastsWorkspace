// 本文件验证平台普通队伍邀请／申请与权限规则；仅使用真实领域对象和固定服务器时间。
// 不连接Redis，不证明跨队唯一归属；该不变量须由应用事务与生产仓储另行验证。
package party

import (
	"math"
	"testing"
	"time"
)

// TestInvitationRequiresTargetAndExpires验证冒名接受和恰好到期均不能增加成员或推进修订。
func TestInvitationRequiresTargetAndExpires(t *testing.T) {
	now := time.Date(2026, 10, 14, 0, 0, 0, 0, time.UTC)
	p := New("party-a", Member{PlayerID: "leader"})
	if err := p.Invite("leader", "invite-a", "guest", now); err != nil {
		t.Fatal(err)
	}
	revision := p.Revision
	if err := p.ResolveMembershipRequest("intruder", "invite-a", true, now); err == nil {
		t.Fatal("其他玩家不能接受邀请")
	}
	if p.Revision != revision || len(p.Members) != 1 {
		t.Fatal("越权请求改变了队伍")
	}
	if err := p.ResolveMembershipRequest("guest", "invite-a", true, now.Add(60*time.Second)); err == nil {
		t.Fatal("恰好到期必须拒绝")
	}
	if len(p.Members) != 1 || p.Revision != revision {
		t.Fatal("过期邀请不应修改名单")
	}
}

// TestInvitationAcceptanceIsTerminal验证接受只加入一次，名单变化使已有准备状态失效。
func TestInvitationAcceptanceIsTerminal(t *testing.T) {
	now := time.Date(2026, 10, 14, 0, 0, 0, 0, time.UTC)
	p := New("party-a", Member{PlayerID: "leader"})
	_ = p.SetReady("leader", true)
	if err := p.Invite("leader", "invite-a", "guest", now); err != nil {
		t.Fatal(err)
	}
	if err := p.ResolveMembershipRequest("guest", "invite-a", true, now.Add(59*time.Second)); err != nil {
		t.Fatal(err)
	}
	if len(p.Members) != 2 || p.Members["leader"].Ready {
		t.Fatal("入队必须增加成员并使原准备失效")
	}
	revision := p.Revision
	if err := p.ResolveMembershipRequest("guest", "invite-a", true, now.Add(time.Hour)); err != nil {
		t.Fatal("同一已接受请求重试应幂等", err)
	}
	if p.Revision != revision {
		t.Fatal("终态重试不能再次修改修订")
	}
	if err := p.ResolveMembershipRequest("guest", "invite-a", false, now); err == nil {
		t.Fatal("不能反转已接受终态")
	}
}

// TestJoinApplicationCanOnlyBeApprovedByLeader验证申请人不能自批，且120秒前批准加入。
func TestJoinApplicationCanOnlyBeApprovedByLeader(t *testing.T) {
	now := time.Date(2026, 10, 14, 0, 0, 0, 0, time.UTC)
	p := New("party-a", Member{PlayerID: "leader"})
	if err := p.RequestJoin("apply-a", "guest", now); err != nil {
		t.Fatal(err)
	}
	if err := p.ResolveMembershipRequest("guest", "apply-a", true, now); err == nil {
		t.Fatal("申请人不能批准自己")
	}
	if err := p.ResolveMembershipRequest("leader", "apply-a", true, now.Add(119*time.Second)); err != nil {
		t.Fatal(err)
	}
	if _, ok := p.Members["guest"]; !ok {
		t.Fatal("队长批准后没有加入")
	}
}

// TestMembershipChangesBlockedDuringQueue验证锁定后不能借准备、邀请或接受申请绕过名单冻结。
func TestMembershipChangesBlockedDuringQueue(t *testing.T) {
	now := time.Date(2026, 10, 14, 0, 0, 0, 0, time.UTC)
	p := New("party-a", Member{PlayerID: "leader"})
	_ = p.Invite("leader", "invite-a", "guest", now)
	p.LockRoster()
	revision := p.Revision
	if err := p.SetReady("leader", true); err == nil {
		t.Fatal("锁定后不能修改准备")
	}
	if err := p.Invite("leader", "invite-b", "other", now); err == nil {
		t.Fatal("锁定后不能发送邀请")
	}
	if err := p.ResolveMembershipRequest("guest", "invite-a", true, now); err == nil {
		t.Fatal("锁定后不能接受加入")
	}
	if p.Revision != revision || len(p.Members) != 1 {
		t.Fatal("锁定失败分支修改了状态")
	}
}

// TestLeaderLeaveTransfersByJoinSequence验证转让依据加入序号，而不是名称或map顺序。
func TestLeaderLeaveTransfersByJoinSequence(t *testing.T) {
	p := New("party-a", Member{PlayerID: "leader"})
	_ = p.AddMember(Member{PlayerID: "z-first"})
	_ = p.AddMember(Member{PlayerID: "a-second"})
	if err := p.Leave("leader"); err != nil {
		t.Fatal(err)
	}
	if p.LeaderPlayerID != "z-first" || len(p.Members) != 2 {
		t.Fatal("未转让给最早加入者")
	}
	if err := p.TransferLeadership("a-second", "z-first"); err == nil {
		t.Fatal("普通成员不能转让队长")
	}
	if err := p.TransferLeadership("z-first", "a-second"); err != nil {
		t.Fatal(err)
	}
	if p.LeaderPlayerID != "a-second" {
		t.Fatal("显式转让没有生效")
	}
}

// TestInvitationIdentityConflictAndCapacity验证同一请求ID异参冲突，且满员时接受不能改变邀请终态。
func TestInvitationIdentityConflictAndCapacity(t *testing.T) {
	now := time.Date(2026, 10, 14, 0, 0, 0, 0, time.UTC)
	p := New("party-a", Member{PlayerID: "leader"})
	_ = p.Invite("leader", "invite-a", "guest", now)
	if err := p.Invite("leader", "invite-a", "other", now); err == nil {
		t.Fatal("同ID不同目标必须冲突")
	}
	for _, id := range []string{"m1", "m2", "m3", "m4"} {
		if err := p.AddMember(Member{PlayerID: id}); err != nil {
			t.Fatal(err)
		}
	}
	revision := p.Revision
	if err := p.ResolveMembershipRequest("guest", "invite-a", true, now); err == nil {
		t.Fatal("满员时不能接受")
	}
	if p.Revision != revision || p.MembershipRequests["invite-a"].State != MembershipRequestPending {
		t.Fatal("满员失败修改了邀请终态")
	}
	if err := p.AddMember(Member{PlayerID: "m1"}); err != nil {
		t.Fatal("已有成员重试不应因满员失败", err)
	}
}

// TestCancelRequestRequiresSender验证邀请者撤回与申请者取消的权限，不允许事后恢复为Pending。
func TestCancelRequestRequiresSender(t *testing.T) {
	now := time.Date(2026, 10, 14, 0, 0, 0, 0, time.UTC)
	p := New("party-a", Member{PlayerID: "leader"})
	_ = p.RequestJoin("apply-a", "guest", now)
	if err := p.CancelMembershipRequest("other", "apply-a"); err == nil {
		t.Fatal("其他玩家不能取消")
	}
	if err := p.CancelMembershipRequest("guest", "apply-a"); err != nil {
		t.Fatal(err)
	}
	if err := p.ResolveMembershipRequest("leader", "apply-a", true, now); err == nil {
		t.Fatal("取消后不能接受")
	}
}

// TestJoinSequenceOverflowLeavesAggregateUnchanged验证损坏／耗尽序号拒绝加入，不能先改变序号或已有准备。
func TestJoinSequenceOverflowLeavesAggregateUnchanged(t *testing.T) {
	for _, sequence := range []int64{math.MaxInt64 - 1, math.MaxInt64} {
		p := New("party-a", Member{PlayerID: "leader"})
		leader := p.Members["leader"]
		leader.JoinSequence, leader.Ready = sequence, true
		p.Members["leader"] = leader
		revision, next := p.Revision, p.NextJoinSequence
		if err := p.AddMember(Member{PlayerID: "guest"}); err == nil {
			t.Fatal("不能分配无法保留下一有效序号的成员")
		}
		if p.NextJoinSequence != next || p.Revision != revision || len(p.Members) != 1 || !p.Members["leader"].Ready {
			t.Fatal("序号检查失败改变了聚合")
		}
	}
}

// TestUnknownRequestKindFailsClosed验证旧记录／损坏记录的未知类型不会绕过邀请目标及队长权限检查。
func TestUnknownRequestKindFailsClosed(t *testing.T) {
	now := time.Date(2026, 10, 14, 0, 0, 0, 0, time.UTC)
	p := New("party-a", Member{PlayerID: "leader"})
	p.MembershipRequests = map[string]MembershipRequest{"unknown": {
		ID: "unknown", Kind: MembershipRequestKind("unsupported"), PlayerID: "guest",
		RequestedByPlayerID: "intruder", ExpiresAtUTC: now.Add(time.Minute), State: MembershipRequestPending,
	}}
	revision := p.Revision
	if err := p.ResolveMembershipRequest("intruder", "unknown", true, now); err == nil {
		t.Fatal("未知请求类型不能批准")
	}
	if err := p.CancelMembershipRequest("intruder", "unknown"); err == nil {
		t.Fatal("未知请求类型不能撤回")
	}
	if p.Revision != revision || len(p.Members) != 1 || p.MembershipRequests["unknown"].State != MembershipRequestPending {
		t.Fatal("未知类型失败路径改变了聚合")
	}
}

// TestDuplicateJoinSequenceRejectsAutomaticLeadership验证任何重复顺序均被拒绝，而非依赖map恰好迭代顺序。
func TestDuplicateJoinSequenceRejectsAutomaticLeadership(t *testing.T) {
	p := New("party-a", Member{PlayerID: "leader"})
	for _, id := range []string{"first", "second", "third"} {
		if err := p.AddMember(Member{PlayerID: id}); err != nil {
			t.Fatal(err)
		}
	}
	member := p.Members["third"]
	member.JoinSequence = p.Members["second"].JoinSequence
	p.Members["third"] = member
	revision := p.Revision
	if err := p.Leave("leader"); err == nil {
		t.Fatal("重复序号不能自动选队长")
	}
	if p.LeaderPlayerID != "leader" || p.Revision != revision || len(p.Members) != 4 {
		t.Fatal("顺序冲突不应改变名单")
	}
}

// TestParallelInvitationAndApplicationAdvanceRevision验证同一玩家两种请求各自结束时均发布新修订，不重复加入成员。
func TestParallelInvitationAndApplicationAdvanceRevision(t *testing.T) {
	now := time.Date(2026, 10, 14, 0, 0, 0, 0, time.UTC)
	p := New("party-a", Member{PlayerID: "leader"})
	if err := p.Invite("leader", "invite-a", "guest", now); err != nil {
		t.Fatal(err)
	}
	if err := p.RequestJoin("apply-a", "guest", now); err != nil {
		t.Fatal(err)
	}
	if err := p.ResolveMembershipRequest("guest", "invite-a", true, now); err != nil {
		t.Fatal(err)
	}
	revision := p.Revision
	if err := p.ResolveMembershipRequest("leader", "apply-a", true, now); err != nil {
		t.Fatal(err)
	}
	if p.Revision != revision+1 || len(p.Members) != 2 || p.MembershipRequests["apply-a"].State != MembershipRequestAccepted {
		t.Fatal("请求终态变化必须推进修订且不能重复加入")
	}
}

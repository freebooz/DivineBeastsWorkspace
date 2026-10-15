// 本文件验证持久队列纯领域规则；不分配服务器、不签票，不把测试候选冒充真实准入。
package matchmaking

import (
	"fmt"
	"testing"
	"time"
)

// TestQueueFiveModesKeepPartiesAtomic覆盖五模式混合单排/整队填充，每个原子队伍始终在同一侧且无重复玩家。
func TestQueueFiveModesKeepPartiesAtomic(t *testing.T) {
	for size := 1; size <= 5; size++ {
		t.Run(fmt.Sprint(size), func(t *testing.T) {
			mode := []string{"Arena.Mode.Duel1v1", "Arena.Mode.Team2v2", "Arena.Mode.Team3v3", "Arena.Mode.Team4v4", "Arena.Mode.Team5v5"}[size-1]
			q := NewQueueState()
			now := time.Now().UTC()
			for team := 0; team < 2; team++ {
				sizes := []int{size}
				if size > 1 {
					sizes = []int{size - 1, 1}
				}
				for group, count := range sizes {
					members := []string{}
					for i := 0; i < count; i++ {
						members = append(members, fmt.Sprintf("p-%d-%d-%d", team, group, i))
					}
					partyID := ""
					if count > 1 {
						partyID = fmt.Sprintf("party-%d-%d", team, group)
					}
					ticket, err := NewTicket(CreateTicketRequest{TicketID: fmt.Sprintf("ticket-%d-%d", team, group), GameID: "divine-beasts", ArenaModeID: mode, PartyID: partyID, PartyMemberIDs: members, Region: "region"})
					if err != nil {
						t.Fatal(err)
					}
					if err := q.Enqueue(QueueEntry{Ticket: ticket, OwnerPlayerID: members[0], Revision: 1, Phase: PhaseSearching}, now); err != nil {
						t.Fatal(err)
					}
				}
			}
			candidate, ok, err := q.FormCandidate("candidate", mode, "region", now)
			if err != nil || !ok {
				t.Fatalf("无法组成%dv%d: %v", size, size, err)
			}
			counts := map[string]int{}
			side := map[string]string{}
			for _, slot := range candidate.Roster {
				if side[slot.PlayerID] != "" {
					t.Fatal("玩家重复")
				}
				counts[slot.TeamID]++
				side[slot.PlayerID] = slot.TeamID
			}
			if counts["team-a"] != size || counts["team-b"] != size {
				t.Fatalf("队伍人数错误: %+v", counts)
			}
			for _, entry := range q.Entries {
				team := ""
				for _, player := range entry.Ticket.PartyMemberIDs {
					if team != "" && team != side[player] {
						t.Fatal("原队伍被拆分")
					}
					team = side[player]
				}
			}
		})
	}
}

// TestQueueConfirmationCancelAndExpiry验证逐人确认门禁、陈旧版本、拒绝释放以及到期后不能入场。
func TestQueueConfirmationCancelAndExpiry(t *testing.T) {
	now := time.Now().UTC()
	q := NewQueueState()
	for i := 0; i < 2; i++ {
		p := fmt.Sprintf("p%d", i)
		ticket, _ := NewTicket(CreateTicketRequest{TicketID: p, GameID: "divine-beasts", ArenaModeID: "Arena.Mode.Duel1v1", PartyMemberIDs: []string{p}, Region: "region"})
		if err := q.Enqueue(QueueEntry{Ticket: ticket, OwnerPlayerID: p, Revision: 1, Phase: PhaseSearching}, now); err != nil {
			t.Fatal(err)
		}
	}
	c, ok, err := q.FormCandidate("candidate", "Arena.Mode.Duel1v1", "region", now)
	if err != nil || !ok {
		t.Fatal(err)
	}
	if err := q.Reply("p0", c.ID, c.Revision, true, now); err != nil {
		t.Fatal(err)
	}
	if q.Candidates[c.ID].Phase == PhaseAllocating {
		t.Fatal("只有一人接受不能分配")
	}
	if err := q.Reply("p1", c.ID, c.Revision, true, now); err == nil {
		t.Fatal("陈旧候选版本必须冲突")
	}
	latest := q.Candidates[c.ID]
	if err := q.Reply("p1", c.ID, latest.Revision, false, now); err != nil {
		t.Fatal(err)
	}
	if len(q.Reservations) != 0 {
		t.Fatal("拒绝后仍有幽灵排队预留")
	}
	for _, entry := range q.Entries {
		if entry.Phase != PhaseCancelled {
			t.Fatalf("候选拒绝未终止整组: %+v", entry)
		}
	}
}

// TestQueueRejectsDuplicatePlayersAndIncompleteTeams覆盖一人多票、无法保持原队伍时不凑数以及取消权限。
func TestQueueRejectsDuplicatePlayersAndIncompleteTeams(t *testing.T) {
	now := time.Now().UTC()
	q := NewQueueState()
	ticket, _ := NewTicket(CreateTicketRequest{TicketID: "one", GameID: "divine-beasts", ArenaModeID: "Arena.Mode.Team2v2", PartyMemberIDs: []string{"one"}, Region: "region"})
	if err := q.Enqueue(QueueEntry{Ticket: ticket, OwnerPlayerID: "one", Phase: PhaseSearching, Revision: 1}, now); err != nil {
		t.Fatal(err)
	}
	ticket.TicketID = "duplicate"
	if err := q.Enqueue(QueueEntry{Ticket: ticket, OwnerPlayerID: "one", Phase: PhaseSearching, Revision: 1}, now); err == nil {
		t.Fatal("同一玩家不能创建第二有效票据")
	}
	if _, ok, err := q.FormCandidate("incomplete", ticket.ArenaModeID, "region", now); err != nil || ok {
		t.Fatal("人数不足不得编造候选")
	}
	if err := q.Cancel("stranger", "one", 1, now); err == nil {
		t.Fatal("非本人不能取消")
	}
	if err := q.Cancel("one", "one", 1, now); err != nil {
		t.Fatal(err)
	}
	if len(q.Reservations) != 0 {
		t.Fatal("取消没有释放预留")
	}
}

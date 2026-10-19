// 本回归在预留已生效但尚未签票的窗口切换真实Boot；相同旧意图不得在新进程上签票或绕过容量。
package gameserver

import (
	"testing"
	"time"
)

func TestWorldReservationReplayRejectsSuccessorBoot(t *testing.T) {
	now := time.Now().UTC()
	r := NewRegistry()
	r.Register(Instance{ID: "village", ServerBootID: "old-boot", RoleID: RoleVillage, Capacity: 2, Status: StatusReady, LastHeartbeat: now})
	if err := r.ReserveWorldTransfer("ticket", "village", "old-boot", 1, now, now.Add(time.Minute)); err != nil {
		t.Fatal(err)
	}
	r.Register(Instance{ID: "village", ServerBootID: "new-boot", RoleID: RoleVillage, Capacity: 2, Status: StatusReady, LastHeartbeat: now})
	if err := r.ReserveWorldTransfer("ticket", "village", "old-boot", 1, now, now.Add(time.Minute)); err == nil {
		t.Fatal("旧返回意图不能继承新Boot")
	}
	current, _ := r.Get("village")
	if current.ReservedPlayers != 0 {
		t.Fatal("旧意图不能污染新Boot容量")
	}
}

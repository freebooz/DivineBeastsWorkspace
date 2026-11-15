package httpadapter

import (
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"divinebeasts/backend/internal/app/gameservercontrol"
	"divinebeasts/backend/internal/modules/gameserver"
	"divinebeasts/backend/internal/modules/match"
	"divinebeasts/backend/internal/modules/servertransfer"
)

func TestServerValidationInventoryReportsEveryCurrentVillageAndArenaBoot(t *testing.T) {
	now := time.Date(2026, 11, 15, 12, 0, 0, 0, time.UTC)
	clock := func() time.Time { return now }
	registry := gameserver.NewRegistry()
	for _, instance := range []gameserver.Instance{
		{ID: "arena-active", ServerBootID: "active-boot", RoleID: gameserver.RoleMainArena, Status: gameserver.StatusActive, LastHeartbeat: now},
		{ID: "arena-starting", ServerBootID: "starting-boot", RoleID: gameserver.RoleMainArena, Status: gameserver.StatusStarting, LastHeartbeat: now},
		{ID: "arena-old", ServerBootID: "old-boot", RoleID: gameserver.RoleMainArena, Status: gameserver.StatusReady, LastHeartbeat: now.Add(-time.Minute)},
		{ID: "village", ServerBootID: "current-boot", RoleID: gameserver.RoleVillage, ExperienceID: "Experience.Village.Tutorial", Status: gameserver.StatusReady, BuildVersion: "1.0.0-test", ProtocolVersion: 1, PublicEndpoint: "127.0.0.1:18782", LastHeartbeat: now},
		{ID: "hub", ServerBootID: "hub-boot", RoleID: gameserver.RoleOpenWorld, Status: gameserver.StatusReady, LastHeartbeat: now},
		{ID: "village-draining", RoleID: gameserver.RoleVillage, Status: gameserver.StatusDraining, LastHeartbeat: now},
	} {
		registry.Register(instance)
	}
	handler := NewGameServerControlHandler(gameservercontrol.NewService(registry,
		servertransfer.NewService([]byte("01234567890123456789012345678901"), clock), match.NewResultService(match.NewMemoryResultStore()), clock), gameServerControlTestToken)
	request := httptest.NewRequest(http.MethodGet, "/internal/v1/gameservers/validation-inventory", nil)
	result := httptest.NewRecorder()
	handler.ServeHTTP(result, request)
	if result.Code != http.StatusUnauthorized {
		t.Fatalf("匿名读取返回 %d，预期 401", result.Code)
	}
	request.Header.Set("Authorization", "Bearer "+gameServerControlTestToken)
	result = httptest.NewRecorder()
	handler.ServeHTTP(result, request)
	var reply struct {
		Instances []struct {
			GameServerID, ServerBootID, ServerRoleID, ExperienceID, Status, BuildVersion, PublicEndpoint string
			ProtocolVersion                                                                              uint32
			HeartbeatUTC                                                                                 time.Time
		}
	}
	if result.Code != http.StatusOK || json.Unmarshal(result.Body.Bytes(), &reply) != nil || len(reply.Instances) != 4 {
		t.Fatalf("预期看到四个本地候选（包括仍可被分配的陈旧竞技实例），状态 %d，内容 %s", result.Code, result.Body.String())
	}
	a, old, b, v := reply.Instances[0], reply.Instances[1], reply.Instances[2], reply.Instances[3]
	if a.GameServerID != "arena-active" || a.Status != "Active" || old.GameServerID != "arena-old" || b.GameServerID != "arena-starting" || b.Status != "Starting" ||
		v.GameServerID != "village" || v.ServerBootID != "current-boot" || v.ServerRoleID != gameserver.RoleVillage ||
		v.ExperienceID != "Experience.Village.Tutorial" || v.Status != "Ready" || v.BuildVersion != "1.0.0-test" ||
		v.ProtocolVersion != 1 || v.PublicEndpoint != "127.0.0.1:18782" || !v.HeartbeatUTC.Equal(now) {
		t.Fatalf("控制面实例证据不吻合：%+v", reply.Instances)
	}
}

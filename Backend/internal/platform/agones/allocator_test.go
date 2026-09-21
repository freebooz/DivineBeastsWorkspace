package agones

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"
)

// TestBuildRequestForMainArena（主竞技场Agones请求构造测试）验证Role、Region、BuildVersion和Match上下文被准确映射。
func TestBuildRequestForMainArena(t *testing.T) {
	request, err := BuildAllocationRequest(AllocationInput{Namespace: "games", RoleID: "GameServer.Role.MainArena", RegionID: "us-west", BuildVersion: "0.2.0", MatchID: "match-1", ArenaModeID: "Arena.Mode.Team5v5"})
	if err != nil {
		t.Fatal(err)
	}
	if request.APIVersion != "allocation.agones.dev/v1" || request.Kind != "GameServerAllocation" {
		t.Fatalf("Agones API信息错误: %+v", request)
	}
	labels := request.Spec.Selectors[0].MatchLabels
	if labels["server-role"] != "main-arena" || labels["region"] != "us-west" || labels["build-version"] != "0.2.0" {
		t.Fatalf("分配选择器错误: %+v", labels)
	}
	if request.Spec.Metadata.Annotations["divinebeasts.io/match-id"] != "match-1" {
		t.Fatal("缺少MatchId Annotation")
	}
}

// TestClientAllocateCallsKubernetesAllocationAPI（Agones分配HTTP测试）验证客户端调用Kubernetes CRD API并解析已分配GameServer。
func TestClientAllocateCallsKubernetesAllocationAPI(t *testing.T) {
	server := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost || r.URL.Path != "/apis/allocation.agones.dev/v1/namespaces/games/gameserverallocations" {
			t.Fatalf("请求路径错误: %s %s", r.Method, r.URL.Path)
		}
		if r.Header.Get("Authorization") != "Bearer token-1" {
			t.Fatalf("Authorization错误: %s", r.Header.Get("Authorization"))
		}
		var request GameServerAllocation
		if err := json.NewDecoder(r.Body).Decode(&request); err != nil {
			t.Fatal(err)
		}
		w.Header().Set("Content-Type", "application/json")
		_ = json.NewEncoder(w).Encode(GameServerAllocation{Status: AllocationStatus{State: "Allocated", GameServerName: "mainarena-abc", Address: "203.0.113.10", Ports: []GameServerPort{{Name: "game", Port: 7777}}}})
	}))
	defer server.Close()

	client := NewClient(ClientConfig{BaseURL: server.URL, BearerToken: "token-1", HTTPClient: server.Client()})
	result, err := client.Allocate(context.Background(), AllocationInput{Namespace: "games", RoleID: "GameServer.Role.MainArena", RegionID: "us-west", BuildVersion: "0.2.0", MatchID: "match-1", ArenaModeID: "Arena.Mode.Team5v5"})
	if err != nil {
		t.Fatal(err)
	}
	if result.GameServerName != "mainarena-abc" || result.Address != "203.0.113.10" || result.Port != 7777 {
		t.Fatalf("分配结果错误: %+v", result)
	}
}

// TestBuildRequestForOpenWorldHub（开放世界大厅/主城分配测试）验证大厅体验使用OpenWorld服务器池。
func TestBuildRequestForOpenWorldHub(t *testing.T) {
	request, err := BuildAllocationRequest(AllocationInput{Namespace: "games", RoleID: "GameServer.Role.OpenWorld", RegionID: "us-west", BuildVersion: "0.8.0"})
	if err != nil {
		t.Fatalf("构造OpenWorld分配请求失败: %v", err)
	}
	if got := request.Spec.Selectors[0].MatchLabels["server-role"]; got != "open-world" {
		t.Fatalf("OpenWorld server-role=%s，期望=open-world", got)
	}
}

package agones

import (
	"context"
	"encoding/json"
	"net/http"
	"net/http/httptest"
	"testing"

	gameservercontract "divinebeasts/backend/internal/contracts/gameserver"
	"divinebeasts/backend/internal/modules/gameserver"
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

// TestBuildRequestForOpenWorldDefault（开放世界默认体验测试）验证OpenWorld角色默认选择Main体验池。
func TestBuildRequestForOpenWorldDefault(t *testing.T) {
	request, err := BuildAllocationRequest(AllocationInput{Namespace: "games", RoleID: "GameServer.Role.OpenWorld", RegionID: "us-west", BuildVersion: "0.8.0"})
	if err != nil {
		t.Fatalf("构造OpenWorld分配请求失败: %v", err)
	}
	if got := request.Spec.Selectors[0].MatchLabels["server-role"]; got != "open-world" {
		t.Fatalf("OpenWorld server-role=%s，期望=open-world", got)
	}
	if got := request.Spec.Selectors[0].MatchLabels["experience"]; got != "Experience.OpenWorld.Main" {
		t.Fatalf("OpenWorld默认Experience=%s，期望=Experience.OpenWorld.Main", got)
	}
}

// TestBuildRequestForOpenWorldHub（大厅体验Agones请求测试）验证大厅体验使用OpenWorld角色标签和正式体验ID。
func TestBuildRequestForOpenWorldHub(t *testing.T) {
	request, err := BuildAllocationRequest(AllocationInput{Namespace: "games", RoleID: gameserver.RoleOpenWorld, ExperienceID: gameservercontract.ExperienceOpenWorldHub, RegionID: "us-west", BuildVersion: "2.0.0"})
	if err != nil {
		t.Fatalf("构造OpenWorld大厅体验分配请求失败：%v", err)
	}
	selector := request.Spec.Selectors[0].MatchLabels
	if selector["server-role"] != "open-world" || selector["experience"] != "Experience.OpenWorld.Hub" {
		t.Fatalf("OpenWorld大厅体验分配标签错误：%+v", selector)
	}
}

// TestFormatGameServerEndpoint（公网端点格式测试）验证 IPv6 括号及不可路由、注入地址拒绝。
func TestFormatGameServerEndpoint(t *testing.T) {
	cases := []struct {
		name string
		host string
		port int
		want string
	}{
		{name: "IPv4", host: "203.0.113.10", port: 30211, want: "203.0.113.10:30211"},
		{name: "IPv6", host: "2001:db8::10", port: 31000, want: "[2001:db8::10]:31000"},
		{name: "DNS", host: "game-01.example.internal", port: 30200, want: "game-01.example.internal:30200"},
		{name: "回环地址", host: "127.0.0.1", port: 30200},
		{name: "未指定IPv6地址", host: "::", port: 30200},
		{name: "多播地址", host: "224.0.0.1", port: 30200},
		{name: "链路本地IPv6", host: "fe80::1", port: 30200},
		{name: "非法域名标签", host: "game..local", port: 30200},
		{name: "伪造数字IP", host: "999.999.999.999", port: 30200},
		{name: "头注入", host: "test.example\r\nInjected", port: 30200},
		{name: "带端口的非法主机", host: "game.example:1", port: 30200},
		{name: "零端口", host: "203.0.113.10", port: 0},
		{name: "端口越界", host: "203.0.113.10", port: 65536},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			got, err := formatGameServerEndpoint(tc.host, tc.port)
			if tc.want == "" {
				if err == nil {
					t.Fatalf("非法 Agones 地址/端口未拒绝：%s:%d", tc.host, tc.port)
				}
				return
			}
			if err != nil || got != tc.want {
				t.Fatalf("Agones 端点格式错误：got=%q want=%q err=%v", got, tc.want, err)
			}
		})
	}
}

// TestAllocateRejectsWrongGamePort（端口选择拒绝测试）
// 验证控制面不会用指标端口或重复 game 端口作为游戏连接地址，异常响应应失败关闭。
func TestAllocateRejectsWrongGamePort(t *testing.T) {
	cases := []struct {
		name    string
		address string
		ports   []GameServerPort
	}{
		{name: "缺少game端口", address: "203.0.113.11",
			ports: []GameServerPort{{Name: "metrics", Port: 9000}}},
		{name: "重复game端口", address: "203.0.113.11",
			ports: []GameServerPort{{Name: "game", Port: 30100}, {Name: "game", Port: 30101}}},
		{name: "game端口越界", address: "203.0.113.11",
			ports: []GameServerPort{{Name: "game", Port: 65536}}},
		{name: "非法IPv6回环", address: "::1",
			ports: []GameServerPort{{Name: "game", Port: 30200}}},
	}
	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			server := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
				w.Header().Set("Content-Type", "application/json")
				_ = json.NewEncoder(w).Encode(GameServerAllocation{
					Status: AllocationStatus{
						State: "Allocated", GameServerName: "mainarena-abc",
						Address: tc.address, Ports: tc.ports,
					},
				})
			}))
			defer server.Close()
			client := NewClient(ClientConfig{BaseURL: server.URL, HTTPClient: server.Client()})
			if _, err := client.Allocate(context.Background(), AllocationInput{
				Namespace: "games", RoleID: gameserver.RoleMainArena,
				RegionID: "us-west", BuildVersion: "0.2.0",
			}); err == nil {
				t.Fatal("非法 Agones 分配结果必须失败关闭")
			}
		})
	}
}

// TestClientAllocationRejectsInsecureURLAndRedirect（连接保护测试）
// 禁止 HTTP 携带 Kubernetes ServiceAccount 凭据，拒绝任何跨地址重定向。
func TestClientAllocationRejectsInsecureURLAndRedirect(t *testing.T) {
	leaked := false
	// 跳转目的地与源位于同一个可信 TLS 测试服务内，避免不同自签名证书
	// 使本该检验重定向策略的测试误通过。
	source := httptest.NewTLSServer(http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if r.URL.Path == "/redirected" {
			leaked = true
			w.WriteHeader(http.StatusOK)
			return
		}
		http.Redirect(w, r, "/redirected", http.StatusTemporaryRedirect)
	}))
	defer source.Close()
	input := AllocationInput{
		Namespace: "games", RoleID: gameserver.RoleMainArena,
		RegionID: "us-west", BuildVersion: "0.2.0",
	}
	client := NewClient(ClientConfig{
		BaseURL: source.URL, BearerToken: "do-not-forward",
		HTTPClient: source.Client(),
	})
	if _, err := client.Allocate(context.Background(), input); err == nil {
		t.Fatal("Agones 分配不得跟随 HTTP 重定向")
	}
	if leaked {
		t.Fatal("敏感 ServiceAccount 请求到达了重定向目标")
	}
	insecure := NewClient(ClientConfig{BaseURL: "http://example.invalid", BearerToken: "test"})
	if _, err := insecure.Allocate(context.Background(), input); err == nil {
		t.Fatal("有 Bearer 凭据时必须拒绝非 TLS 的 Kubernetes API")
	}
}


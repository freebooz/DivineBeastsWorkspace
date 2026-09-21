// Package agones（Agones游戏服务器分配适配器）通过Kubernetes CRD API原子分配Dedicated GameServer。
// 本实现只依赖Go标准库，避免为单一Allocation调用把整个Kubernetes客户端SDK引入业务进程。
package agones

import (
	"bytes"
	"context"
	"encoding/json"
	"errors"
	"fmt"
	"io"
	"net/http"
	"net/url"
	"strings"
	"time"

	"divinebeasts/backend/internal/modules/gameserver"
)

// AllocationInput（Agones分配输入）描述GameServerControlService对专用服务器的选择条件。
type AllocationInput struct {
	Namespace    string // Namespace（Agones GameServer所在Kubernetes命名空间）。
	RoleID       string // RoleID（OpenWorld/Village/MainArena服务器角色）。
	ExperienceID string // ExperienceID（目标Experience，用于区分OpenWorld.Hub/OpenWorld.Main/Village等服务器池）。
	RegionID     string // RegionID（部署区域ID）。
	BuildVersion string // BuildVersion（GameServer构建版本）。
	MatchID      string // MatchID（MainArena比赛ID；非竞技分配可为空）。
	ArenaModeID  string // ArenaModeID（1v1至5v5竞技模式ID）。
}

// ObjectMeta（Agones分配元数据）表示分配请求要写入目标GameServer的标签和注解。
type ObjectMeta struct {
	Labels      map[string]string `json:"labels,omitempty"`      // Labels（写入目标GameServer的标签）。
	Annotations map[string]string `json:"annotations,omitempty"` // Annotations（写入目标GameServer的注解）。
}

// GameServerSelector（游戏服务器选择器）通过Label筛选可分配实例。
type GameServerSelector struct {
	MatchLabels     map[string]string `json:"matchLabels,omitempty"`     // MatchLabels（必须同时满足的GameServer标签）。
	GameServerState string            `json:"gameServerState,omitempty"` // GameServerState（默认Ready，可显式指定）。
}

// AllocationSpec（Agones分配规范）使用Selectors按顺序筛选GameServer，并附加比赛上下文元数据。
type AllocationSpec struct {
	Selectors  []GameServerSelector `json:"selectors,omitempty"`  // Selectors（有序GameServer选择器；优先使用，替代已废弃required/preferred）。
	Scheduling string               `json:"scheduling,omitempty"` // Scheduling（Packed或Distributed调度策略）。
	Metadata   ObjectMeta           `json:"metadata,omitempty"`   // Metadata（成功分配后写入目标GameServer的业务元数据）。
}

// GameServerPort（Agones返回的游戏端口）描述已分配服务器可供客户端连接的端口。
type GameServerPort struct {
	Name string `json:"name,omitempty"` // Name（端口名称，例如game）。
	Port int    `json:"port"`           // Port（外部可连接端口）。
}

// AllocationStatus（Agones分配状态）包含原子分配后的GameServer名称、地址和端口。
type AllocationStatus struct {
	State          string           `json:"state,omitempty"`          // State（Allocated或UnAllocated）。
	GameServerName string           `json:"gameServerName,omitempty"` // GameServerName（被分配的GameServer资源名称）。
	Address        string           `json:"address,omitempty"`        // Address（GameServer主网络地址）。
	Ports          []GameServerPort `json:"ports,omitempty"`          // Ports（GameServer对外端口列表）。
}

// GameServerAllocation（Agones GameServerAllocation资源）对应allocation.agones.dev/v1 CRD请求与响应。
type GameServerAllocation struct {
	APIVersion string           `json:"apiVersion"`       // APIVersion（固定allocation.agones.dev/v1）。
	Kind       string           `json:"kind"`             // Kind（固定GameServerAllocation）。
	Spec       AllocationSpec   `json:"spec"`             // Spec（分配筛选与元数据）。
	Status     AllocationStatus `json:"status,omitempty"` // Status（Agones返回的分配结果）。
}

// AllocationResult（游戏服务器分配结果）是GameServerControlService真正需要的精简结果。
type AllocationResult struct {
	GameServerName string // GameServerName（Agones GameServer资源名称）。
	Address        string // Address（客户端连接地址）。
	Port           int    // Port（客户端游戏端口）。
}

// BuildAllocationRequest（构造Agones分配请求）把项目RoleID转换为稳定Label选择器。
func BuildAllocationRequest(input AllocationInput) (GameServerAllocation, error) {
	if input.Namespace == "" || input.RoleID == "" || input.RegionID == "" || input.BuildVersion == "" {
		return GameServerAllocation{}, errors.New("Agones分配的Namespace、RoleID、RegionID和BuildVersion不能为空")
	}
	if input.ExperienceID == "" {
		switch input.RoleID {
		case gameserver.RoleOpenWorld:
			input.ExperienceID = "Experience.OpenWorld.Main"
		case gameserver.RoleVillage:
			input.ExperienceID = "Experience.Village.Main"
		case gameserver.RoleMainArena:
			input.ExperienceID = "Experience.MainArena.Main"
		}
	}
	roleLabel, err := roleLabelValue(input.RoleID)
	if err != nil {
		return GameServerAllocation{}, err
	}
	annotations := map[string]string{}
	if input.MatchID != "" {
		annotations["divinebeasts.io/match-id"] = input.MatchID
	}
	if input.ArenaModeID != "" {
		annotations["divinebeasts.io/arena-mode-id"] = input.ArenaModeID
	}
	return GameServerAllocation{
		APIVersion: "allocation.agones.dev/v1",
		Kind:       "GameServerAllocation",
		Spec: AllocationSpec{
			Selectors: []GameServerSelector{{
				MatchLabels: map[string]string{
					"server-role":   roleLabel,
					"experience":    input.ExperienceID,
					"region":        input.RegionID,
					"build-version": input.BuildVersion,
				},
				GameServerState: "Ready",
			}},
			Scheduling: "Packed",
			Metadata: ObjectMeta{
				Labels:      map[string]string{"server-role": roleLabel, "experience": input.ExperienceID},
				Annotations: annotations,
			},
		},
	}, nil
}

func roleLabelValue(roleID string) (string, error) {
	switch roleID {
	case "GameServer.Role.OpenWorld":
		return "open-world", nil
	case "GameServer.Role.Village":
		return "village", nil
	case "GameServer.Role.MainArena":
		return "main-arena", nil
	default:
		return "", fmt.Errorf("未知GameServer RoleID: %s", roleID)
	}
}

// ClientConfig（Agones分配客户端配置）描述Kubernetes API访问方式。
type ClientConfig struct {
	BaseURL     string       // BaseURL（Kubernetes API地址，例如https://kubernetes.default.svc）。
	BearerToken string       // BearerToken（ServiceAccount访问令牌）。
	HTTPClient  *http.Client // HTTPClient（可注入自定义TLS/连接池；为空时使用默认超时客户端）。
}

// Client（Agones分配客户端）调用Kubernetes GameServerAllocation CRD API。
type Client struct {
	baseURL     string
	bearerToken string
	httpClient  *http.Client
}

// NewClient（创建Agones分配客户端）创建可复用HTTP连接池客户端。
func NewClient(config ClientConfig) *Client {
	client := config.HTTPClient
	if client == nil {
		client = &http.Client{Timeout: 5 * time.Second}
	}
	return &Client{baseURL: strings.TrimRight(config.BaseURL, "/"), bearerToken: config.BearerToken, httpClient: client}
}

// Allocate（原子分配GameServer）向Agones创建临时GameServerAllocation资源并解析Allocated结果。
func (c *Client) Allocate(ctx context.Context, input AllocationInput) (AllocationResult, error) {
	allocation, err := BuildAllocationRequest(input)
	if err != nil {
		return AllocationResult{}, err
	}
	body, err := json.Marshal(allocation)
	if err != nil {
		return AllocationResult{}, fmt.Errorf("序列化Agones分配请求失败: %w", err)
	}
	if c.baseURL == "" {
		return AllocationResult{}, errors.New("Agones Kubernetes API BaseURL不能为空")
	}
	endpoint := c.baseURL + "/apis/allocation.agones.dev/v1/namespaces/" + url.PathEscape(input.Namespace) + "/gameserverallocations"
	req, err := http.NewRequestWithContext(ctx, http.MethodPost, endpoint, bytes.NewReader(body))
	if err != nil {
		return AllocationResult{}, err
	}
	req.Header.Set("Content-Type", "application/json")
	if c.bearerToken != "" {
		req.Header.Set("Authorization", "Bearer "+c.bearerToken)
	}
	resp, err := c.httpClient.Do(req)
	if err != nil {
		return AllocationResult{}, fmt.Errorf("调用Agones分配API失败: %w", err)
	}
	defer resp.Body.Close()
	responseBody, err := io.ReadAll(io.LimitReader(resp.Body, 1<<20))
	if err != nil {
		return AllocationResult{}, fmt.Errorf("读取Agones分配响应失败: %w", err)
	}
	if resp.StatusCode < 200 || resp.StatusCode >= 300 {
		return AllocationResult{}, fmt.Errorf("Agones分配API返回HTTP %d: %s", resp.StatusCode, string(responseBody))
	}
	var result GameServerAllocation
	if err := json.Unmarshal(responseBody, &result); err != nil {
		return AllocationResult{}, fmt.Errorf("解析Agones分配响应失败: %w", err)
	}
	if result.Status.State != "Allocated" || result.Status.GameServerName == "" || result.Status.Address == "" {
		return AllocationResult{}, fmt.Errorf("AGONES_NO_SERVER_ALLOCATED: state=%s", result.Status.State)
	}
	port := 0
	for _, candidate := range result.Status.Ports {
		if candidate.Name == "game" {
			port = candidate.Port
			break
		}
		if port == 0 {
			port = candidate.Port
		}
	}
	if port <= 0 {
		return AllocationResult{}, errors.New("Agones分配结果缺少可用GameServer端口")
	}
	return AllocationResult{GameServerName: result.Status.GameServerName, Address: result.Status.Address, Port: port}, nil
}

// RegistryBackedAllocator（Agones+Registry分配器）先通过Agones原子选择MainArena实例，再把结果同步到Backend Registry。
// OpenWorld/Village常驻服务器池仍建议直接使用RegistryAllocator，避免每名玩家都触发Agones GameServerAllocation。
type RegistryBackedAllocator struct {
	client       *Client
	registry     *gameserver.Registry
	namespace    string
	buildVersion string
}

// NewRegistryBackedAllocator（创建Agones生产分配器）创建MainArena生产分配适配器。
func NewRegistryBackedAllocator(client *Client, registry *gameserver.Registry, namespace, buildVersion string) *RegistryBackedAllocator {
	if client == nil || registry == nil || namespace == "" || buildVersion == "" {
		panic("Agones RegistryBackedAllocator依赖不能为空")
	}
	return &RegistryBackedAllocator{client: client, registry: registry, namespace: namespace, buildVersion: buildVersion}
}

// Allocate（分配MainArena服务器）调用Agones并确认所选实例已经注册到Backend控制面。
func (a *RegistryBackedAllocator) Allocate(ctx context.Context, req gameserver.AllocationRequest) (gameserver.Instance, error) {
	if req.RoleID != gameserver.RoleMainArena {
		return gameserver.Instance{}, errors.New("AGONES_ROLE_UNSUPPORTED: RegistryBackedAllocator仅用于MainArena")
	}
	result, err := a.client.Allocate(ctx, AllocationInput{Namespace: a.namespace, RoleID: req.RoleID, ExperienceID: req.ExperienceID, RegionID: req.RegionID, BuildVersion: a.buildVersion, MatchID: req.MatchID})
	if err != nil {
		return gameserver.Instance{}, err
	}
	allocated, err := a.registry.AllocateSpecific(result.GameServerName, req)
	if err != nil {
		return gameserver.Instance{}, err
	}
	// Agones返回的公网地址是分配时最可信连接地址，覆盖注册阶段可能仍是Pod内地址的Endpoint。
	allocated.PublicEndpoint = fmt.Sprintf("%s:%d", result.Address, result.Port)
	a.registry.Register(allocated)
	return allocated, nil
}

// Release（释放MainArena服务器）清理Backend侧Match绑定；Agones实例生命周期由Dedicated Server SDK/Fleet回收策略负责。
func (a *RegistryBackedAllocator) Release(_ context.Context, gameServerID string) error {
	return a.registry.Release(gameServerID)
}

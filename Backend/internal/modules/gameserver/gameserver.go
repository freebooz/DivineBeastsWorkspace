// Package gameserver（游戏服务器控制领域）管理Dedicated Server注册、心跳、状态和实例分配。
package gameserver

import (
	"errors"
	"sort"
	"sync"
	"time"
)

const (
	RoleOpenWorld = "GameServer.Role.OpenWorld" // RoleOpenWorld（开放世界服务器角色，包含大厅/主城/野外）。
	RoleVillage   = "GameServer.Role.Village"   // RoleVillage（新手村服务器角色）。
	RoleMainArena = "GameServer.Role.MainArena" // RoleMainArena（主竞技场服务器角色）。
)

// IsKnownRole（是否已知服务器角色）确保控制面只接受当前正式三类Dedicated Server角色。
func IsKnownRole(roleID string) bool {
	switch roleID {
	case RoleOpenWorld, RoleVillage, RoleMainArena:
		return true
	default:
		return false
	}
}

// Status（游戏服务器状态）描述实例是否可接受新玩家或新比赛。
type Status string

const (
	StatusStarting  Status = "Starting"  // StatusStarting（启动中）。
	StatusReady     Status = "Ready"     // StatusReady（已就绪）。
	StatusActive    Status = "Active"    // StatusActive（活动中）。
	StatusDraining  Status = "Draining"  // StatusDraining（排空中）。
	StatusUnhealthy Status = "Unhealthy" // StatusUnhealthy（不健康）。
	StatusStopping  Status = "Stopping"  // StatusStopping（停止中）。
)

// Instance（游戏服务器实例）是Backend控制平面的权威快速状态快照。
type Instance struct {
	ID              string    // ID（GameServer实例ID）。
	RoleID          string    // RoleID（服务器角色ID：OpenWorld/Village/MainArena）。
	ExperienceID    string    // ExperienceID（当前实例承载体验）。
	RegionID        string    // RegionID（部署区域ID）。
	ClusterID       string    // ClusterID（Kubernetes/Agones集群ID）。
	NodeID          string    // NodeID（承载节点ID）。
	WorldID         string    // WorldID（当前世界、区域或地图逻辑ID）。
	PublicEndpoint  string    // PublicEndpoint（客户端连接地址）。
	BuildVersion    string    // BuildVersion（GameServer构建版本）。
	ProtocolVersion uint32    // ProtocolVersion（UE实时网络协议版本）。
	Capacity        int       // Capacity（最大玩家容量）。
	CurrentPlayers  int       // CurrentPlayers（当前已连接玩家数）。
	ReservedPlayers int       // ReservedPlayers（已签发迁移但尚未在Heartbeat确认的容量预留）。
	Status          Status    // Status（服务器生命周期状态）。
	CurrentMatchID  string    // CurrentMatchID（MainArena绑定比赛ID）。
	LastHeartbeat   time.Time // LastHeartbeat（最近一次心跳时间）。
}

// AllocationRequest（服务器分配请求）按角色、体验、区域、世界与容量选择实例。
type AllocationRequest struct {
	RoleID           string // RoleID（目标服务器角色ID）。
	ExperienceID     string // ExperienceID（目标体验ID）。
	RegionID         string // RegionID（首选部署区域）。
	WorldID          string // WorldID（目标世界ID；为空表示不限定）。
	RequiredCapacity int    // RequiredCapacity（所需玩家容量）。
	MatchID          string // MatchID（MainArena比赛ID，其他角色必须为空）。
}

// Registry（游戏服务器注册表）保存可快速访问的GameServer状态。
// 生产环境可同步Redis，但领域本身不依赖具体缓存实现。
type Registry struct {
	mu        sync.RWMutex
	instances map[string]Instance
}

// NewRegistry（创建服务器注册表）创建线程安全注册表。
func NewRegistry() *Registry { return &Registry{instances: map[string]Instance{}} }

// Register（注册服务器）新增或刷新GameServer实例信息。
func (r *Registry) Register(instance Instance) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if instance.Status == "" {
		instance.Status = StatusStarting
	}
	if previous, ok := r.instances[instance.ID]; ok {
		// 注册刷新时保留Backend侧尚未完成的容量预留和比赛绑定，避免Heartbeat/重注册造成超卖。
		instance.ReservedPlayers = previous.ReservedPlayers
		if instance.CurrentMatchID == "" {
			instance.CurrentMatchID = previous.CurrentMatchID
		}
	}
	r.instances[instance.ID] = instance
}

// Get（读取服务器）返回服务器状态快照。
func (r *Registry) Get(id string) (Instance, bool) {
	r.mu.RLock()
	defer r.mu.RUnlock()
	value, ok := r.instances[id]
	return value, ok
}

// Heartbeat（更新心跳）更新玩家数、状态和最后心跳时间。
func (r *Registry) Heartbeat(id string, players int, status Status, at time.Time) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	instance, ok := r.instances[id]
	if !ok {
		return errors.New("GAME_SERVER_NOT_FOUND: GameServer实例不存在")
	}
	if players < 0 || players > instance.Capacity {
		return errors.New("GAME_SERVER_PLAYER_COUNT_INVALID: CurrentPlayers超出容量范围")
	}
	instance.CurrentPlayers = players
	instance.Status = status
	instance.LastHeartbeat = at
	// Heartbeat是Dedicated Server的权威在线人数；已经成功进入服务器的玩家不再占用Backend预留。
	if instance.ReservedPlayers > 0 {
		free := instance.Capacity - instance.CurrentPlayers
		if instance.ReservedPlayers > free {
			instance.ReservedPlayers = free
		}
	}
	r.instances[id] = instance
	return nil
}

// SetReady（标记就绪）将已完成启动和地图加载的GameServer标记为Ready。
func (r *Registry) SetReady(id string) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	instance, ok := r.instances[id]
	if !ok {
		return errors.New("GAME_SERVER_NOT_FOUND: GameServer实例不存在")
	}
	instance.Status = StatusReady
	r.instances[id] = instance
	return nil
}

// Drain（进入排空）禁止实例继续接受新分配，但允许已有玩家或比赛结束。
func (r *Registry) Drain(id string) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	instance, ok := r.instances[id]
	if !ok {
		return errors.New("GAME_SERVER_NOT_FOUND: GameServer实例不存在")
	}
	instance.Status = StatusDraining
	r.instances[id] = instance
	return nil
}

// Allocate（分配服务器）使用Best-Fit（最小剩余容量）策略。
// MainArena是一场比赛独占一个实例；OpenWorld/Village允许同一世界实例连续接收多个玩家并预留容量。
func (r *Registry) Allocate(req AllocationRequest) (Instance, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	if req.RequiredCapacity <= 0 {
		return Instance{}, errors.New("RequiredCapacity必须大于0")
	}
	candidates := make([]Instance, 0)
	for _, instance := range r.instances {
		if instance.RoleID != req.RoleID || instance.RegionID != req.RegionID {
			continue
		}
		if req.ExperienceID != "" && instance.ExperienceID != req.ExperienceID {
			continue
		}
		if req.WorldID != "" && instance.WorldID != req.WorldID {
			continue
		}
		if req.RoleID == RoleMainArena {
			if instance.Status != StatusReady || instance.CurrentMatchID != "" {
				continue
			}
		} else if instance.Status != StatusReady && instance.Status != StatusActive {
			continue
		}
		available := instance.Capacity - instance.CurrentPlayers - instance.ReservedPlayers
		if available < req.RequiredCapacity {
			continue
		}
		candidates = append(candidates, instance)
	}
	if len(candidates) == 0 {
		return Instance{}, errors.New("GAME_SERVER_NO_CAPACITY: 当前没有可用服务器容量")
	}
	sort.Slice(candidates, func(i, j int) bool {
		freeI := candidates[i].Capacity - candidates[i].CurrentPlayers - candidates[i].ReservedPlayers
		freeJ := candidates[j].Capacity - candidates[j].CurrentPlayers - candidates[j].ReservedPlayers
		if freeI == freeJ {
			return candidates[i].ID < candidates[j].ID
		}
		return freeI < freeJ
	})
	selected := candidates[0]
	selected.Status = StatusActive
	if req.RoleID == RoleMainArena {
		selected.CurrentMatchID = req.MatchID
	} else {
		selected.ReservedPlayers += req.RequiredCapacity
	}
	r.instances[selected.ID] = selected
	return selected, nil
}

// AllocateSpecific（确认指定GameServer的分配）用于Agones已经挑选出实例后，把Backend注册表状态同步为Allocated/Active。
func (r *Registry) AllocateSpecific(id string, req AllocationRequest) (Instance, error) {
	r.mu.Lock()
	defer r.mu.Unlock()
	instance, ok := r.instances[id]
	if !ok {
		return Instance{}, errors.New("GAME_SERVER_NOT_FOUND: Agones返回的GameServer尚未注册到Backend")
	}
	if instance.RoleID != req.RoleID || instance.RegionID != req.RegionID {
		return Instance{}, errors.New("GAME_SERVER_ALLOCATION_MISMATCH: Agones实例角色或区域不匹配")
	}
	if req.ExperienceID != "" && instance.ExperienceID != req.ExperienceID {
		return Instance{}, errors.New("GAME_SERVER_EXPERIENCE_MISMATCH: Agones实例Experience不匹配")
	}
	if instance.Capacity-instance.CurrentPlayers-instance.ReservedPlayers < req.RequiredCapacity {
		return Instance{}, errors.New("GAME_SERVER_NO_CAPACITY: Agones实例容量不足")
	}
	instance.Status = StatusActive
	if req.RoleID == RoleMainArena {
		if instance.CurrentMatchID != "" && instance.CurrentMatchID != req.MatchID {
			return Instance{}, errors.New("GAME_SERVER_ALREADY_BOUND: MainArena已绑定其他Match")
		}
		instance.CurrentMatchID = req.MatchID
	} else {
		instance.ReservedPlayers += req.RequiredCapacity
	}
	r.instances[id] = instance
	return instance, nil
}

// CommitReservation（提交一个世界迁移容量预留）在迁移票据被目标服务器成功消费后释放预留计数。
func (r *Registry) CommitReservation(id string, count int) error {
	if count <= 0 {
		return errors.New("Reservation count必须大于0")
	}
	r.mu.Lock()
	defer r.mu.Unlock()
	instance, ok := r.instances[id]
	if !ok {
		return errors.New("GAME_SERVER_NOT_FOUND: GameServer实例不存在")
	}
	if instance.ReservedPlayers >= count {
		instance.ReservedPlayers -= count
	} else {
		instance.ReservedPlayers = 0
	}
	r.instances[id] = instance
	return nil
}

// ReleaseReservation（释放未完成的世界迁移容量预留）供签票失败或取消迁移时使用。
func (r *Registry) ReleaseReservation(id string, count int) error {
	return r.CommitReservation(id, count)
}

// Release（释放服务器）结束MainArena比赛后清理Match绑定并恢复Ready状态。
func (r *Registry) Release(id string) error {
	r.mu.Lock()
	defer r.mu.Unlock()
	instance, ok := r.instances[id]
	if !ok {
		return errors.New("GAME_SERVER_NOT_FOUND: GameServer实例不存在")
	}
	instance.CurrentMatchID = ""
	instance.CurrentPlayers = 0
	instance.ReservedPlayers = 0
	if instance.Status != StatusDraining && instance.Status != StatusStopping {
		instance.Status = StatusReady
	}
	r.instances[id] = instance
	return nil
}

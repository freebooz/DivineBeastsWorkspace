package gameserver

import "context"

// Allocator（服务器分配器端口）隔离应用层与具体分配实现。
// 本地环境使用RegistryAllocator；生产MainArena可使用AgonesAllocator。
type Allocator interface {
	Allocate(ctx context.Context, req AllocationRequest) (Instance, error)
	Release(ctx context.Context, gameServerID string) error
}

// RegistryAllocator（注册表分配器）直接使用进程内Registry进行容量选择，适合本地开发和常驻世界服务器池。
type RegistryAllocator struct{ Registry *Registry }

// NewRegistryAllocator（创建注册表分配器）创建本地分配器。
func NewRegistryAllocator(registry *Registry) *RegistryAllocator {
	if registry == nil {
		panic("GameServer Registry不能为空")
	}
	return &RegistryAllocator{Registry: registry}
}

// Allocate（分配服务器）委托Registry执行Best-Fit选择。
func (a *RegistryAllocator) Allocate(_ context.Context, req AllocationRequest) (Instance, error) {
	return a.Registry.Allocate(req)
}

// Release（释放服务器）委托Registry清理比赛绑定和容量预留。
func (a *RegistryAllocator) Release(_ context.Context, gameServerID string) error {
	return a.Registry.Release(gameServerID)
}

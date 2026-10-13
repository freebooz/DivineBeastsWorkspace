package playerdata

import (
	"context"

	"divinebeasts/backend/internal/modules/inventory"
)

// InventoryService（背包服务端口）让PlayerDataService托管长期背包，而不把背包规则复制进玩家资料领域。
type InventoryService interface {
	GetSnapshot(context.Context, string) (inventory.Snapshot, error)
	GetOperation(context.Context, string, string) (inventory.MutationResult, error)
	Move(context.Context, string, inventory.MoveCommand) (inventory.MutationResult, error)
	Split(context.Context, string, inventory.SplitCommand) (inventory.MutationResult, error)
	Merge(context.Context, string, inventory.MergeCommand) (inventory.MutationResult, error)
	SetQuickbar(context.Context, string, inventory.QuickbarCommand) (inventory.MutationResult, error)
	ClearQuickbar(context.Context, string, inventory.QuickbarCommand) (inventory.MutationResult, error)
	Probe(context.Context) error
}

// TrustedInventoryService（可信长期背包授权写端口）与普通InventoryService分开。
// 对外HTTP Gateway不得通过普通AttachInventory暴露Grant/Consume。
type TrustedInventoryService interface {
	GrantTrusted(context.Context, string, inventory.GrantCommand) (inventory.MutationResult, error)
	ConsumeTrusted(context.Context, string, inventory.ConsumeCommand) (inventory.MutationResult, error)
}

// AttachTrustedInventory（显式装配可信端口）只能由完成服务端身份验证的组合根调用。
// nil默认关闭，不由普通登录/查询接口自动启用。
func (s *Service) AttachTrustedInventory(service TrustedInventoryService) {
	if s != nil {
		s.trustedInventory = service
	}
}

// GrantTrustedInventory（权威服务器奖励入口）不对公网友好Gateway暴露。
func (s *Service) GrantTrustedInventory(ctx context.Context, playerID string, cmd inventory.GrantCommand) (inventory.MutationResult, error) {
	if s == nil || s.trustedInventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.trustedInventory.GrantTrusted(ctx, playerID, cmd)
}

// ConsumeTrustedInventory（权威使用消耗入口）不经过客户端原生Move/Merge/Quickbar通道。
func (s *Service) ConsumeTrustedInventory(ctx context.Context, playerID string, cmd inventory.ConsumeCommand) (inventory.MutationResult, error) {
	if s == nil || s.trustedInventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.trustedInventory.ConsumeTrusted(ctx, playerID, cmd)
}

// AttachInventory（装配背包服务）只允许Composition Root显式调用；nil表示当前部署未启用背包。
func (s *Service) AttachInventory(service InventoryService) {
	if s != nil {
		s.inventory = service
	}
}

func (s *Service) GetInventorySnapshot(ctx context.Context, playerID string) (inventory.Snapshot, error) {
	if s == nil || s.inventory == nil {
		return inventory.Snapshot{}, unavailable()
	}
	return s.inventory.GetSnapshot(ctx, playerID)
}

func (s *Service) GetInventoryOperation(ctx context.Context, playerID, operationID string) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.GetOperation(ctx, playerID, operationID)
}

func (s *Service) MoveInventory(ctx context.Context, playerID string, command inventory.MoveCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.Move(ctx, playerID, command)
}

func (s *Service) SplitInventory(ctx context.Context, playerID string, command inventory.SplitCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.Split(ctx, playerID, command)
}

func (s *Service) MergeInventory(ctx context.Context, playerID string, command inventory.MergeCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.Merge(ctx, playerID, command)
}

func (s *Service) SetInventoryQuickbar(ctx context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.SetQuickbar(ctx, playerID, command)
}

func (s *Service) ClearInventoryQuickbar(ctx context.Context, playerID string, command inventory.QuickbarCommand) (inventory.MutationResult, error) {
	if s == nil || s.inventory == nil {
		return inventory.MutationResult{}, unavailable()
	}
	return s.inventory.ClearQuickbar(ctx, playerID, command)
}

func (s *Service) ProbeInventory(ctx context.Context) error {
	if s == nil || s.inventory == nil {
		return unavailable()
	}
	return s.inventory.Probe(ctx)
}
